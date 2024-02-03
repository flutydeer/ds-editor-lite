//
// Created by fluty on 2024/2/3.
//

#ifndef IUNIQUE_H
#define IUNIQUE_H

#include "IdGenerator.h"

class IUnique {
public:
    IUnique() {
        m_id = IdGenerator::instance()->id();
    }
    IUnique(int id) : m_id(id) {
    }
    int id() const {
        return m_id;
    }

protected:
    int m_id;
};

#endif // IUNIQUE_H
