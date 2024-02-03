//
// Created by fluty on 2024/2/3.
//

#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include "Singleton.h"

class IdGenerator : public Singleton<IdGenerator> {
public:
    int id() {
        m_id++;
        return m_id;
    }

private:
    int m_id = -1;
};

#endif // IDGENERATOR_H
