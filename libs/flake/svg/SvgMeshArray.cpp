/*
 *  Copyright (c) 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "SvgMeshArray.h"

#include <KoPathSegment.h>
#include <kis_global.h>
#include <QDebug>

SvgMeshArray::SvgMeshArray()
{
}

SvgMeshArray::SvgMeshArray(const SvgMeshArray& other)
{
    for (const auto& row: other.m_array) {
        newRow();
        for (const auto& patch: row) {
            m_array.last().append(new SvgMeshPatch(*patch));
        }
    }
}

SvgMeshArray::~SvgMeshArray()
{
    for (auto& row: m_array) {
        for (auto& patch: row) {
            delete patch;
        }
    }
}

void SvgMeshArray::newRow()
{
    m_array << QVector<SvgMeshPatch*>();
}

void SvgMeshArray::createDefaultMesh(const int nrows,
                                     const int ncols,
                                     const QColor color,
                                     const QSizeF size)
{
    // individual patch size should be:
    qreal patchWidth  = size.width()  / ncols;
    qreal patchHeight = size.height() / nrows;

    // normalize
    patchWidth  /= size.width();
    patchHeight /= size.height();

    QRectF start(0, 0, patchWidth, patchHeight);

    QColor colors[2] = {Qt::white, color};

    for (int irow = 0; irow < nrows; ++irow) {
        newRow();

        for (int icol = 0; icol < ncols; ++icol) {
            SvgMeshPatch *patch = new SvgMeshPatch(start.topLeft());
            // alternate between colors
            int index = (irow + icol) % 2;

            patch->addStop({start.topLeft(), start.topRight()},
                           colors[index],
                           SvgMeshPatch::Top);

            index = (index + 1) % 2;
            patch->addStop({start.topRight(), start.bottomRight()},
                           colors[index],
                           SvgMeshPatch::Right);

            index = (index + 1) % 2;
            patch->addStop({start.bottomRight(), start.bottomLeft()},
                           colors[index],
                           SvgMeshPatch::Bottom);

            index = (index + 1) % 2;
            patch->addStop({start.bottomLeft(), start.topLeft()},
                           colors[index],
                           SvgMeshPatch::Left);

            m_array.last().append(patch);

            // TopRight of the previous patch in this row
            start.setX(patch->getStop(SvgMeshPatch::Right).point.x());
            start.setWidth(patchWidth);
        }

        // BottomLeft of the patch is the starting point for new row
        start.setTopLeft(m_array.last().first()->getStop(SvgMeshPatch::Left).point);
        start.setSize({patchWidth, patchHeight});
    }
}

bool SvgMeshArray::addPatch(QList<QPair<QString, QColor>> stops, const QPointF initialPoint)
{
    // This is function is full of edge-case landmines, please run TestMeshArray after any changes
    if (stops.size() > 4 || stops.size() < 2)
        return false;

    SvgMeshPatch *patch = new SvgMeshPatch(initialPoint);

     m_array.last().append(patch);

    int irow = m_array.size() - 1;
    int icol = m_array.last().size() - 1;

    if (irow == 0 && icol == 0) {
        patch->addStop(stops[0].first, stops[0].second, SvgMeshPatch::Top);
        stops.removeFirst();
    } else if (irow == 0) {
        // For first row, parse patches
        patch->addStop(stops[0].first, getColor(SvgMeshPatch::Right, irow, icol - 1), SvgMeshPatch::Top);
        stops.removeFirst();
    } else {
        // path is already defined for rows >= 1
        QColor color = getStop(SvgMeshPatch::Left, irow - 1, icol).color;

        QList<QPointF> points = getPath(SvgMeshPatch::Bottom, irow - 1, icol);
        std::reverse(points.begin(), points.end());

        patch->addStop(points, color, SvgMeshPatch::Top);
    }

    if (irow > 0) {
        patch->addStop(stops[0].first, getColor(SvgMeshPatch::Bottom, irow - 1, icol), SvgMeshPatch::Right);
        stops.removeFirst();
    } else {
        patch->addStop(stops[0].first, stops[0].second, SvgMeshPatch::Right);
        stops.removeFirst();
    }

    if (icol > 0) {
        patch->addStop(
                stops[0].first,
                stops[0].second,
                SvgMeshPatch::Bottom,
                true, getStop(SvgMeshPatch::Bottom, irow, icol - 1).point);
        stops.removeFirst();
    } else {
        patch->addStop(stops[0].first, stops[0].second, SvgMeshPatch::Bottom);
        stops.removeFirst();
    }

    // last stop
    if (icol == 0) {
        // if stop is in the 0th column, parse path
        patch->addStop(
                stops[0].first,
                stops[0].second,
                SvgMeshPatch::Left,
                true, getStop(SvgMeshPatch::Top, irow, icol).point);
        stops.removeFirst();
    } else {
        QColor color = getStop(SvgMeshPatch::Bottom, irow, icol - 1).color;

        // reuse Right side of the previous patch
        QList<QPointF> points = getPath(SvgMeshPatch::Right, irow, icol - 1);
        std::reverse(points.begin(), points.end());

        patch->addStop(points, color, SvgMeshPatch::Left);
    }
    return true;
}

SvgMeshStop SvgMeshArray::getStop(const SvgMeshPatch::Type edge, const int row, const int col) const
{
    KIS_ASSERT(row < m_array.size() && col < m_array[row].size()
            && row >= 0 && col >= 0);

    SvgMeshPatch *patch = m_array[row][col];
    SvgMeshStop node = patch->getStop(edge);

    if (node.isValid()) {
        return node;
    }

    switch (patch->countPoints()) {
    case 3:
    case 2:
        if (edge == SvgMeshPatch::Top)
            return getStop(SvgMeshPatch::Left, row - 1, col);
        else if (edge == SvgMeshPatch::Left)
            return getStop(SvgMeshPatch::Bottom, row, col - 1);
    }
    assert(false);
}

QList<QPointF> SvgMeshArray::getPath(const SvgMeshPatch::Type edge, const int row, const int col) const
{
    KIS_ASSERT(row < m_array.size() && col < m_array[row].size()
            && row >= 0 && col >= 0);

    return m_array[row][col]->getPathSegment(edge).controlPoints();
}

SvgMeshPatch* SvgMeshArray::getPatch(const int row, const int col) const
{
    KIS_ASSERT(row < m_array.size() && col < m_array[row].size()
            && row >= 0 && col >= 0);

    return m_array[row][col];
}

int SvgMeshArray::numRows() const
{
    return m_array.size();
}

int SvgMeshArray::numColumns() const
{
    if (m_array.isEmpty())
        return 0;
    return m_array.first().size();
}

void SvgMeshArray::setTransform(const QTransform& matrix)
{
    for (auto& row: m_array) {
        for (auto& patch: row) {
            patch->setTransform(matrix);
        }
    }
}

QRectF SvgMeshArray::boundingRect() const
{
    KIS_ASSERT(numRows() > 0 && numColumns() > 0);

    QPointF topLeft = m_array[0][0]->boundingRect().topLeft();
    QPointF bottomRight = m_array.last().last()->boundingRect().bottomRight();

    // mesharray may be backwards, in which case we might get the right most value
    // but we need topLeft for things to work as expected
    for (int i = 0; i < numRows(); ++i) {
        for (int j = 0; j < numColumns(); ++j) {
            QPointF left  = m_array[i][j]->boundingRect().topLeft();
            if (left.x() < topLeft.x()) {
                topLeft.rx() = left.x();
            }
            if ( left.y() < topLeft.y()) {
                topLeft.ry() = left.y();
            }

            QPointF right = m_array[i][j]->boundingRect().bottomRight();
            if (bottomRight.x() < right.x()) {
                bottomRight.rx() = right.x();
            }
            if (bottomRight.y() < right.y()) {
                bottomRight.ry() = right.y();
            }
        }
    }

    // return extremas
    return QRectF(topLeft, bottomRight);
}
QColor SvgMeshArray::getColor(SvgMeshPatch::Type edge, int row, int col) const
{
    return getStop(edge, row, col).color;
}

