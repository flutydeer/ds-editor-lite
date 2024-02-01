//
// Created by fluty on 2024/2/1.
//

#ifndef TESTTIMEOBJECT_H
#define TESTTIMEOBJECT_H

#include "../../gui/Utils/IComparable.h"

class TestTimeObject : public IComparable {
public:
    explicit TestTimeObject(int tick) : m_tick(tick) {
    }
    ~TestTimeObject() override = default;
    int tick() const {
        return m_tick;
    }

    int compareTo(IComparable *other) override {
        auto obj = dynamic_cast<TestTimeObject *>(other);
        if (tick() < obj->tick())
            return -1;
        if (tick() > obj->tick())
            return 1;
        return 0;
    }

private:
    int m_tick = 0;
};



#endif // TESTTIMEOBJECT_H
