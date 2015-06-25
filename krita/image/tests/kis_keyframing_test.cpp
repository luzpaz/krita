/*
 *  Copyright (c) 2015 Jouni Pentikäinen <joupent@gmail.com>
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

#include "kis_keyframing_test.h"
#include <qtest_kde.h>
#include <qsignalspy.h>

#include "kis_keyframe_channel.h"
#include "kis_scalar_keyframe_channel.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_node.h"

#include <KoColorSpaceRegistry.h>

void KisKeyframingTest::initTestCase()
{
    cs = KoColorSpaceRegistry::instance()->rgb8();

    red = new quint8[cs->pixelSize()];
    green = new quint8[cs->pixelSize()];
    blue = new quint8[cs->pixelSize()];

    cs->fromQColor(Qt::red, red);
    cs->fromQColor(Qt::green, green);
    cs->fromQColor(Qt::blue, blue);
}

void KisKeyframingTest::cleanupTestCase()
{
    delete red;
    delete green;
    delete blue;
}

void KisKeyframingTest::testScalarChannel()
{
    KisScalarKeyframeChannel *channel = new KisScalarKeyframeChannel(KoID("", ""), 0, -17, 31);
    KisKeyframe *key;
    bool ok;

    QCOMPARE(channel->hasScalarValue(), true);
    QCOMPARE(channel->minScalarValue(), -17.0);
    QCOMPARE(channel->maxScalarValue(),  31.0);

    QVERIFY(channel->keyframeAt(0) == 0);

    // Adding new keyframe

    key = channel->addKeyframe(42);
    channel->setScalarValue(key, 7.0);

    key = channel->keyframeAt(42);
    QCOMPARE(channel->scalarValue(key), 7.0);

    // Adding a keyframe where one exists

    KisKeyframe *key2 = channel->addKeyframe(42);
    QVERIFY(key2 == key);

    // Copying a keyframe

    key2 = channel->copyKeyframe(key, 13);
    QVERIFY(key2 != 0);
    QVERIFY(channel->keyframeAt(13) == key2);
    QCOMPARE(channel->scalarValue(key2), 7.0);

    // Moving keyframes

    ok = channel->moveKeyframe(key, 10);
    QCOMPARE(ok, true);
    QVERIFY(channel->keyframeAt(42) == 0);

    key = channel->keyframeAt(10);
    QCOMPARE(channel->scalarValue(key), 7.0);

    // Moving a keyframe where another one exists
    ok = channel->moveKeyframe(key, 13);
    QCOMPARE(ok, false);
    QVERIFY(channel->keyframeAt(13) == key2);

    channel->deleteKeyframe(key);

    QVERIFY(channel->keyframeAt(10) == 0);

    delete channel;
}

void KisKeyframingTest::testRasterChannel()
{
    struct TestingKeyframingDefaultBounds : public KisDefaultBoundsBase {
        TestingKeyframingDefaultBounds() : m_time(0) {}

        QRect bounds() const {
            return QRect(0,0,100,100);
        }

        bool wrapAroundMode() const {
            return false;
        }

        int currentLevelOfDetail() const {
            return 0;
        }

        int currentTime() const {
            return m_time;
        }

        bool externalFrameActive() const {
            return false;
        }

        void testingSetTime(int time) {
            m_time = time;
        }

    private:
        int m_time;
    };

    TestingKeyframingDefaultBounds *bounds = new TestingKeyframingDefaultBounds();

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(bounds);

    KisRasterKeyframeChannel * channel = dev->createKeyframeChannel(KoID(), 0);

    QCOMPARE(channel->hasScalarValue(), false);
    QCOMPARE(channel->keyframes().length(), 1);
    QCOMPARE(dev->frames().count(), 1);
    QCOMPARE(channel->frameIdAt(0), 0);
    QVERIFY(channel->keyframeAt(0) != 0);

    KisKeyframe * key_0 = channel->keyframeAt(0);

    // New keyframe

    KisKeyframe * key_10 = channel->addKeyframe(10);
    QCOMPARE(channel->keyframes().length(), 2);
    QCOMPARE(dev->frames().count(), 2);
    QVERIFY(channel->frameIdAt(10) != 0);

    dev->fill(0, 0, 512, 512, red);
    QImage thumb1a = dev->createThumbnail(50, 50);

    bounds->testingSetTime(10);

    dev->fill(0, 0, 512, 512, green);
    QImage thumb2a = dev->createThumbnail(50, 50);

    bounds->testingSetTime(0);
    QImage thumb1b = dev->createThumbnail(50, 50);

    QVERIFY(thumb2a != thumb1a);
    QVERIFY(thumb1b == thumb1a);

    // Duplicate keyframe

    KisKeyframe * key_20 = channel->copyKeyframe(key_0, 20);
    bounds->testingSetTime(20);
    QImage thumb3a = dev->createThumbnail(50, 50);

    QVERIFY(thumb3a == thumb1b);

    dev->fill(0, 0, 512, 512, blue);
    QImage thumb3b = dev->createThumbnail(50, 50);

    bounds->testingSetTime(0);
    QImage thumb1c = dev->createThumbnail(50, 50);

    QVERIFY(thumb3b != thumb3a);
    QVERIFY(thumb1c == thumb1b);

    // Delete keyrame
    QCOMPARE(channel->keyframes().count(), 3);
    QCOMPARE(dev->frames().count(), 3);

    channel->deleteKeyframe(key_0);
    QCOMPARE(channel->keyframes().count(), 2);
    QCOMPARE(dev->frames().count(), 2);
    QVERIFY(channel->keyframeAt(0) == 0);

    channel->deleteKeyframe(key_20);
    QCOMPARE(channel->keyframes().count(), 1);
    QCOMPARE(dev->frames().count(), 1);
    QVERIFY(channel->keyframeAt(20) == 0);

    // Last remaining keyframe cannot be deleted
    channel->deleteKeyframe(key_10);
    QCOMPARE(channel->keyframes().count(), 1);
    QCOMPARE(dev->frames().count(), 1);
    QVERIFY(channel->keyframeAt(10) != 0);

    // Fetching current keyframe before the first one should
    // return the first keyframe
    QCOMPARE(channel->frameIdAt(0), (int)key_10->value());
}

void KisKeyframingTest::testChannelSignals()
{
    KisScalarKeyframeChannel *channel = new KisScalarKeyframeChannel(KoID("", ""), 0, -17, 31);
    KisKeyframe *key;
    KisKeyframe *resKey;

    qRegisterMetaType<KisKeyframe*>("KisKeyframePtr");
    QSignalSpy spyPreAdd(channel, SIGNAL(sigKeyframeAboutToBeAdded(KisKeyframe*)));
    QSignalSpy spyPostAdd(channel, SIGNAL(sigKeyframeAdded(KisKeyframe*)));

    QSignalSpy spyPreRemove(channel, SIGNAL(sigKeyframeAboutToBeRemoved(KisKeyframe*)));
    QSignalSpy spyPostRemove(channel, SIGNAL(sigKeyframeRemoved(KisKeyframe*)));

    QSignalSpy spyPreMove(channel, SIGNAL(sigKeyframeAboutToBeMoved(KisKeyframe*,int)));
    QSignalSpy spyPostMove(channel, SIGNAL(sigKeyframeMoved(KisKeyframe*, int)));

    QVERIFY(spyPreAdd.isValid());
    QVERIFY(spyPostAdd.isValid());
    QVERIFY(spyPreRemove.isValid());
    QVERIFY(spyPostRemove.isValid());
    QVERIFY(spyPreMove.isValid());
    QVERIFY(spyPostMove.isValid());

    // Adding a keyframe

    QCOMPARE(spyPreAdd.count(), 0);
    QCOMPARE(spyPostAdd.count(), 0);

    key = channel->addKeyframe(10);

    QCOMPARE(spyPreAdd.count(), 1);
    QCOMPARE(spyPostAdd.count(), 1);

    resKey = spyPreAdd.at(0).at(0).value<KisKeyframe*>();
    QVERIFY(resKey == key);
    resKey = spyPostAdd.at(0).at(0).value<KisKeyframe*>();
    QVERIFY(resKey == key);

    // Moving a keyframe

    QCOMPARE(spyPreMove.count(), 0);
    QCOMPARE(spyPostMove.count(), 0);
    channel->moveKeyframe(key, 15);
    QCOMPARE(spyPreMove.count(), 1);
    QCOMPARE(spyPostMove.count(), 1);

    resKey = spyPreMove.at(0).at(0).value<KisKeyframe*>();
    QVERIFY(resKey == key);
    QCOMPARE(spyPreMove.at(0).at(1).toInt(), 15);
    resKey = spyPostMove.at(0).at(0).value<KisKeyframe*>();
    QVERIFY(resKey == key);

    // No-op move (no signals)

    channel->moveKeyframe(key, 15);
    QCOMPARE(spyPreMove.count(), 1);
    QCOMPARE(spyPostMove.count(), 1);

    // Deleting a keyframe

    QCOMPARE(spyPreRemove.count(), 0);
    QCOMPARE(spyPostRemove.count(), 0);
    channel->deleteKeyframe(key);
    QCOMPARE(spyPreRemove.count(), 1);
    QCOMPARE(spyPostRemove.count(), 1);

    delete channel;
}

QTEST_KDEMAIN(KisKeyframingTest, NoGUI)
#include "kis_keyframing_test.moc"
