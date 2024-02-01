//
// Created by fluty on 2024/2/1.
//

#include <QDebug>

#include "TestTimeObject.h"
#include "../../gui/Utils/SerialList.h"

int main(int argc, char *argv[]) {
    TestTimeObject obj1(11);
    TestTimeObject obj2(45);
    TestTimeObject obj3(14);
    TestTimeObject obj4(11);

    qDebug() << "obj1" << obj1.tick() << "obj2:" << obj2.tick() << obj1.compareTo(&obj2);
    qDebug() << "obj2" << obj2.tick() << "obj3:" << obj3.tick() << obj2.compareTo(&obj3);
    qDebug() << "obj1" << obj1.tick() << "obj4:" << obj4.tick() << obj1.compareTo(&obj4);

    SerialList<TestTimeObject> list;
    list.add(&obj1);
    list.add(&obj2);
    list.add(&obj3);
    list.add(&obj4);
    // list.remove(&obj2);

    // for (const auto item : list.items()) {
    //     qDebug() << item->tick();
    // }
    for (int i = 0; i < list.count(); i++)
        qDebug() << list.at(i)->tick();
    return 0;
}