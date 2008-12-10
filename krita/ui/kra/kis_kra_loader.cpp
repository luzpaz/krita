/* This file is part of the KDE project
 * Copyright (C) Boudewijn Rempt <boud@valdyas.org>, (C) 2007
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kra/kis_kra_loader.h"

#include "kis_kra_tags.h"
#include "kis_kra_load_visitor.h"

#include <KoStore.h>
#include <KoColorSpaceRegistry.h>
#include <KoIccColorProfile.h>
#include <KoDocumentInfo.h>

#include "kis_doc2.h"
#include "kis_config.h"

#include <filter/kis_filter.h>
#include <filter/kis_filter_registry.h>
#include <generator/kis_generator.h>
#include <generator/kis_generator_layer.h>
#include <generator/kis_generator_registry.h>
#include <kis_adjustment_layer.h>
#include <kis_annotation.h>
#include <kis_base_node.h>
#include <kis_clone_layer.h>
#include <kis_debug.h>
#include <kis_external_layer_iface.h>
#include <kis_filter_mask.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_name_server.h>
#include <kis_paint_device_action.h>
#include <kis_paint_layer.h>
#include <kis_selection.h>
#include <kis_selection_mask.h>
#include <kis_shape_layer.h>
#include <kis_transformation_mask.h>
#include <kis_transparency_mask.h>


using namespace KRA;

class KisKraLoader::Private
{
public:

    KisDoc2* document;
    QString imageName; // used to be stored in the image, is now in the documentInfo block
    QString imageComment; // used to be stored in the image, is now in the documentInfo block
    QMap<KisNode*, QString> layerFilenames; // temp storage during loading
    int syntaxVersion; // version of the fileformat we are loading

};

KisKraLoader::KisKraLoader(KisDoc2 * document, int syntaxVersion)
    : m_d(new Private())
{
    m_d->document = document;
    m_d->syntaxVersion = syntaxVersion;
}


KisKraLoader::~KisKraLoader()
{
    delete m_d;
}


KisImageSP KisKraLoader::loadXML(const KoXmlElement& element)
{

    KisConfig cfg;
    QString attr;
    KoXmlNode node;
    KoXmlNode child;
    KisImageSP img = 0;
    QString name;
    qint32 width;
    qint32 height;
    QString description;
    QString profileProductName;
    double xres;
    double yres;
    QString colorspacename;
    const KoColorSpace * cs;

    if ((attr = element.attribute(MIME)) == NATIVE_MIMETYPE) {

        if ((m_d->imageName = element.attribute(NAME)).isNull())
            return KisImageSP(0);

        if ((attr = element.attribute(WIDTH)).isNull()) {
            return KisImageSP(0);
        }
        width = attr.toInt();

        if ((attr = element.attribute(HEIGHT)).isNull()) {
            return KisImageSP(0);
        }
        height = attr.toInt();

        m_d->imageComment = element.attribute(DESCRIPTION);

        xres = 100.0 / 72.0;
        if (!(attr = element.attribute(X_RESOLUTION)).isNull()) {
            if (attr.toDouble() > 1.0)
                xres = attr.toDouble() / 72.0;
        }

        yres = 100.0 / 72.0;
        if (!(attr = element.attribute(Y_RESOLUTION)).isNull()) {
            if (attr.toDouble() > 1.0)
                yres = attr.toDouble() / 72.0;
        }

        if ( ( colorspacename = element.attribute( COLORSPACE_NAME ) ).isNull() ) {
            // An old file: take a reasonable default.
            // Krita didn't support anything else in those
            // days anyway.
            colorspacename = "RGBA";
        }

        // A hack for an old colorspacename
        if (colorspacename  == "Grayscale + Alpha")
            colorspacename  = "GRAYA";

        if ((profileProductName = element.attribute(PROFILE)).isNull()) {
            // no mention of profile so get default profile
            cs = KoColorSpaceRegistry::instance()->colorSpace(colorspacename, "");
        } else {
            cs = KoColorSpaceRegistry::instance()->colorSpace(colorspacename, profileProductName);
        }

        if (cs == 0) {
            warnFile << "Could not open colorspace";
            return KisImageSP(0);
        }

        img = new KisImage(m_d->document->undoAdapter(), width, height, cs, name);

#ifdef __GNUC__
#warning "KisKraLoader::loadXML: check whether image->lock still works with the top-down update "
#endif
        img->lock();

        img->setResolution(xres, yres);

        loadNodes(element, img, const_cast<KisGroupLayer*>( img->rootLayer().data() ));

        img->unlock();

    }

    return img;
}

void KisKraLoader::loadBinaryData(KoStore * store, KisImageSP img, const QString & uri, bool external)
{
    // Load the layers data
    KisKraLoadVisitor visitor(img, store, m_d->layerFilenames, m_d->imageName, m_d->syntaxVersion);

    if (external)
        visitor.setExternalUri(uri);

    img->lock();

    img->rootLayer()->accept(visitor);


    // annotations
    // exif
    QString location = external ? QString::null : uri;
    location += m_d->imageName + EXIF_PATH;
    if (store->hasFile(location)) {
        QByteArray data;
        store->open(location);
        data = store->read(store->size());
        store->close();
        img->addAnnotation(KisAnnotationSP(new KisAnnotation("exif", "", data)));
    }

    // icc profile
    location = external ? QString::null : uri;
    location += m_d->imageName + ICC_PATH;
    if (store->hasFile(location)) {
        QByteArray data;
        store->open(location);
        data = store->read(store->size());
        store->close();
        img->setProfile(new KoIccColorProfile(data));
    }


    if (m_d->document->documentInfo()->aboutInfo("title").isNull())
        m_d->document->documentInfo()->setAboutInfo("title", m_d->imageName);
    if (m_d->document->documentInfo()->aboutInfo("comment").isNull())
        m_d->document->documentInfo()->setAboutInfo("comment", m_d->imageComment);

    img->unlock();
}

KisNode* KisKraLoader::loadNodes(const KoXmlElement& element, KisImageSP img, KisNode* parent)
{
    KoXmlNode node = element.lastChild();
    KoXmlNode child;

    if ( !node.isNull() ) {
        if ( node.isElement() ) {
            if ( node.nodeName() == LAYERS || node.nodeName() == MASKS ) {
                for ( child = node.lastChild(); !child.isNull(); child = child.previousSibling() ) {
                    KisNode* node = loadNode( child.toElement(), img );

                    if ( !node ) {
                        warnFile << "Could not load node";
#ifdef __GNUC__
#warning "KisKraLoader::loadNodes: report node load failures back to the user!"
#endif
                    } else {
                        img->nextLayerName(); // Make sure the nameserver is current with the number of nodes.
                        img->addNode(node, parent);
                        if ( node->inherits( "KisLayer" ) ) {
                            loadNodes( child.toElement(), img, node );
                        }

                    }
                }
            }
        }
    }

    return parent;
}

KisNode* KisKraLoader::loadNode(const KoXmlElement& element, KisImageSP img)
{
    // Nota bene: If you add new properties to layers, you should
    // ALWAYS define a default value in case the property is not
    // present in the layer definition: this helps a LOT with backward
    // compatibility.

    QString name = element.attribute(NAME, "No Name");

    qint32 x = element.attribute(X, "0").toInt();
    qint32 y = element.attribute(Y, "0").toInt();

    qint32 opacity = element.attribute(OPACITY, QString::number( OPACITY_OPAQUE ) ).toInt();
    if ( opacity < OPACITY_TRANSPARENT ) opacity = OPACITY_TRANSPARENT;
    if ( opacity > OPACITY_OPAQUE ) opacity = OPACITY_OPAQUE;

    const KoColorSpace* colorSpace = 0;
    if ( ( element.attribute( COLORSPACE_NAME ) ).isNull() )
        colorSpace = img->colorSpace();
    else
        // use default profile - it will be replaced later in completeLoading
        colorSpace = KoColorSpaceRegistry::instance()->colorSpace(element.attribute( COLORSPACE_NAME ), "");

    bool visible = element.attribute(VISIBLE, "1") == "0" ? false : true;
    bool locked = element.attribute(LOCKED, "0") == "0" ? false : true;

    // Now find out the layer type and do specific handling
    QString attr = element.attribute(NODE_TYPE);
    KisNode* node = 0;

    if (attr.isNull())
        node = loadPaintLayer(element, img, name, colorSpace, opacity);
    else if (attr == PAINT_LAYER)
        node = loadPaintLayer(element, img, name, colorSpace, opacity);
    else if (attr == GROUP_LAYER)
        node = loadGroupLayer(element, img, name, colorSpace, opacity);
    else if (attr == ADJUSTMENT_LAYER )
        node = loadAdjustmentLayer(element, img, name, colorSpace, opacity);
    else if (attr == SHAPE_LAYER )
        node = loadShapeLayer(element, img, name, colorSpace, opacity);
    else if ( attr == GENERATOR_LAYER )
        node = loadGeneratorLayer( element, img, name, colorSpace, opacity);
    else if ( attr == CLONE_LAYER )
        node = loadCloneLayer( element, img, name, colorSpace, opacity);
    else if ( attr == FILTER_MASK )
        node = loadFilterMask( element );
    else if ( attr == TRANSPARENCY_MASK )
        node = loadTransparencyMask( element );
    else if ( attr == TRANSFORMATION_MASK )
        node = loadTransformationMask( element );
    else if ( attr == SELECTION_MASK )
        node = loadSelectionMask( img, element );
    else
        warnKrita << "Trying to load layer of unsupported type " << attr;

    // Loading the node went wrong. Return empty node and leave to
    // upstream to complain to the user
    if ( !node ) return 0;

    node->setVisible( visible );
    node->setUserLocked( locked );
    node->setX( x );
    node->setY( y );
    node->setName( name );

    if ( node->inherits( "KisLayer" ) ) {
        KisLayer* layer = qobject_cast<KisLayer*>( node );

        QString channelFlagsString = element.attribute( CHANNEL_FLAGS );
        if ( !channelFlagsString.isEmpty() ) {
            QBitArray channelFlags( channelFlagsString.length() );
            for ( int i = 0; i < channelFlagsString.length(); ++i ) {
                channelFlags.setBit(i, channelFlagsString[i] == '1' );
            }
            layer->setChannelFlags( channelFlags );
        }

        QString compositeOpName = element.attribute( COMPOSITE_OP, "normal" );
        layer->setCompositeOp( colorSpace->compositeOp( compositeOpName ) );
    }

    if ( element.attribute( FILE_NAME ).isNull() )
        m_d->layerFilenames[node] = name;
    else
        m_d->layerFilenames[node] = QString( element.attribute( FILE_NAME ) );

    return node;
}


KisNode* KisKraLoader::loadPaintLayer(const KoXmlElement& element, KisImageSP img,
                                      const QString& name, const KoColorSpace* cs, quint32 opacity)
{
    QString attr;
    KisPaintLayer* layer;

    QString colorspacename;
    QString profileProductName;

    layer = new KisPaintLayer(img, name, opacity, cs);
    Q_CHECK_PTR(layer);

    // Load exif info
    /*TODO: write and use the legacy stuff to load that exif tag
      for( KoXmlNode node = element.firstChild(); !node.isNull(); node = node.nextSibling() )
      {
      KoXmlElement e = node.toElement();
      if ( !e.isNull() && e.tagName() == "ExifInfo" )
      {
      layer->paintDevice()->exifInfo()->load(e);
      }
      }*/
    // TODO load metadata

    return layer;

}

KisNode* KisKraLoader::loadGroupLayer(const KoXmlElement& element, KisImageSP img,
                                      const QString& name, const KoColorSpace* cs, quint32 opacity )
{
    QString attr;
    KisGroupLayer* layer;

    layer = new KisGroupLayer(img, name, opacity);
    Q_CHECK_PTR(layer);

    return layer;

}

KisNode* KisKraLoader::loadAdjustmentLayer(const KoXmlElement& element, KisImageSP img,
                                           const QString& name, const KoColorSpace* cs, quint32 opacity)
{
    // XXX: do something with filterversion?

    QString attr;
    KisAdjustmentLayer* layer;
    QString filtername;

    if ((filtername = element.attribute( FILTER_NAME )).isNull()) {
        // XXX: Invalid adjustmentlayer! We should warn about it!
        warnFile << "No filter in adjustment layer";
        return 0;
    }

    KisFilterSP f = KisFilterRegistry::instance()->value(filtername);
    if (!f) {
        warnFile << "No filter for filtername" << filtername << "";
        return 0; // XXX: We don't have this filter. We should warn about it!
    }

    KisFilterConfiguration * kfc = f->defaultConfiguration(0);

    // We'll load the configuration and the selection later.
    layer = new KisAdjustmentLayer(img, name, kfc, 0);
    Q_CHECK_PTR(layer);

    layer->setOpacity( opacity );

    return layer;

}


KisNode* KisKraLoader::loadShapeLayer(const KoXmlElement& element, KisImageSP img,
                                      const QString& name, const KoColorSpace* cs, quint32 opacity)
{
    QString attr;

    KisShapeLayer* layer = new KisShapeLayer(0, img, name, opacity);
    Q_CHECK_PTR(layer);

    return layer;

}


KisNode* KisKraLoader::loadGeneratorLayer(const KoXmlElement& element, KisImageSP img,
                                           const QString& name, const KoColorSpace* cs, quint32 opacity)
{
    // XXX: do something with generator version?
    KisGeneratorLayer* layer;
    QString generatorname = element.attribute( GENERATOR_NAME );

    if (generatorname.isNull()) {
        // XXX: Invalid generator layer! We should warn about it!
        warnFile << "No generator in generator layer";
        return 0;
    }

    KisGeneratorSP generator = KisGeneratorRegistry::instance()->value(generatorname);
    if (!generator) {
        warnFile << "No generator for generatorname" << generatorname << "";
        return 0; // XXX: We don't have this generator. We should warn about it!
    }

    KisFilterConfiguration * kgc = generator->defaultConfiguration(0);

    // We'll load the configuration and the selection later.
    layer = new KisGeneratorLayer(img, name, kgc, 0);
    Q_CHECK_PTR(layer);

    layer->setOpacity( opacity );

    return layer;

}

KisNode* KisKraLoader::loadCloneLayer(const KoXmlElement& element, KisImageSP img,
                                      const QString& name, const KoColorSpace* cs, quint32 opacity)
{
    KisCloneLayer* layer = new KisCloneLayer(0, img, name, opacity);

    if ( ( element.attribute( CLONE_FROM ) ).isNull() ) {
        return 0;
    }
    else {
        layer->setCopyFromName( element.attribute( CLONE_FROM ) );
    }

    if ( ( element.attribute( CLONE_TYPE ) ).isNull() ) {
        return 0;
    }
    else {
        layer->setCopyType( ( CopyLayerType ) element.attribute( CLONE_TYPE ).toInt() );
    }

    return layer;
}


KisNode* KisKraLoader::loadFilterMask(const KoXmlElement& element )
{
    QString attr;
    KisFilterMask* mask;
    QString filtername;

    // XXX: should we check the version?

    if ((filtername = element.attribute( FILTER_NAME )).isNull()) {
        // XXX: Invalid filter layer! We should warn about it!
        warnFile << "No filter in filter layer";
        return 0;
    }

    KisFilterSP f = KisFilterRegistry::instance()->value(filtername);
    if (!f) {
        warnFile << "No filter for filtername" << filtername << "";
        return 0; // XXX: We don't have this filter. We should warn about it!
    }

    KisFilterConfiguration * kfc = f->defaultConfiguration(0);

    // We'll load the configuration and the selection later.
    mask = new KisFilterMask();
    mask->setFilter( kfc );
    Q_CHECK_PTR( mask );

    return mask;
}

KisNode* KisKraLoader::loadTransparencyMask(const KoXmlElement& element)
{
    KisTransparencyMask* mask = new KisTransparencyMask();
    Q_CHECK_PTR( mask );

    return mask;
}

KisNode* KisKraLoader::loadTransformationMask(const KoXmlElement& element)
{
    KisTransformationMask* mask = new KisTransformationMask();
    Q_CHECK_PTR( mask );


    mask->setXScale( element.attribute(X_SCALE, "1.0").toDouble() );
    mask->setYScale( element.attribute(Y_SCALE, "1.0").toDouble() );
    mask->setXShear( element.attribute(X_SHEAR, "1.0").toDouble() );
    mask->setYShear( element.attribute(Y_SHEAR, "1.0").toDouble() );
    mask->setRotation( element.attribute(ROTATION, "0.0").toDouble() );
    mask->setXTranslation( element.attribute(X_TRANSLATION, "0").toInt() );
    mask->setYTranslation( element.attribute(Y_TRANSLATION, "0").toInt() );

    KisFilterStrategy* filterStrategy = 0;
    QString filterStrategyName = element.attribute(FILTER_STATEGY, "Mitchell" );
    filterStrategy = KisFilterStrategyRegistry::instance()->get( filterStrategyName );
    mask->setFilterStrategy( filterStrategy );



    return mask;
}

KisNode* KisKraLoader::loadSelectionMask(KisImageSP img, const KoXmlElement& element)
{
    KisSelectionMask* mask = new KisSelectionMask(img);
    Q_CHECK_PTR( mask );

    return mask;
}

