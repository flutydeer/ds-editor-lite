//
// Created by fluty on 2024/2/1.
//

#include <QDebug>

#include "TestTimeObject.h"
#include "../../app/Utils/OverlappableSerialList.h"
#include "../../app/Utils/VolumeUtils.h"

int main(int argc, char *argv[]) {
    TestTimeObject obj1(960, 1920);
    TestTimeObject obj2(480, 960);
    TestTimeObject obj3(720, 1440);
    TestTimeObject obj4(0, 480);

    qDebug() << "obj1" << obj1.start() << "obj2:" << obj2.start() << obj1.compareTo(&obj2);
    qDebug() << "obj2" << obj2.start() << "obj3:" << obj3.start() << obj2.compareTo(&obj3);
    qDebug() << "obj1" << obj1.start() << "obj4:" << obj4.start() << obj1.compareTo(&obj4);

    OverlappableSerialList<TestTimeObject> list;
    list.add(&obj1);
    list.add(&obj2);
    list.add(&obj3);
    list.add(&obj4);
    // list.remove(&obj2);

    for (const auto item : list)
        qDebug() << item->start() << item->start() + item->length();
    // qDebug() << obj3.isOverlappedWith(&obj2);
    qDebug() << list.hasOverlappedItem();
    for (const auto item : list.overlappedItems())
        qDebug() << item->start() << item->start() + item->length();

    qDebug() << VolumeUtils::dBToLinear(6) << VolumeUtils::dBToLinear(-60)
    << VolumeUtils::dBToLinear(0) << VolumeUtils::dBToLinear(-70);
    qDebug() << VolumeUtils::linearTodB(1) << VolumeUtils::linearTodB(0.00001);

    return 0;
}