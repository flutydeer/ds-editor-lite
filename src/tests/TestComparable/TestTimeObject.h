//
// Created by fluty on 2024/2/1.
//

#ifndef TESTTIMEOBJECT_H
#define TESTTIMEOBJECT_H

#include "../../gui/Utils/IOverlapable.h"

class TestTimeObject : public IOverlapable {
public:
    explicit TestTimeObject(int start, int length) : m_start(start), m_lenght(length) {
    }
    ~TestTimeObject() override = default;
    int start() const {
        return m_start;
    }
    int length() const {
        return m_lenght;
    }

    int compareTo(IOverlapable *obj) override {
        auto other = dynamic_cast<TestTimeObject *>(obj);
        if (start() < other->start())
            return -1;
        if (start() > other->start())
            return 1;
        return 0;
    }

    bool isOverlappedWith(IOverlapable *obj) override {
        auto other = dynamic_cast<TestTimeObject *>(obj);
        if (other->start() + other->length() <= start() || start() + length() <= other->start())
            return false;
        return true;
    }

private:
    int m_start = 0;
    int m_lenght = 0;
};



#endif // TESTTIMEOBJECT_H
