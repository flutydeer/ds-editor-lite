//
// Created by fluty on 24-10-25.
//

#ifndef IHASHPROVIDER_H
#define IHASHPROVIDER_H

#include "Utils/Macros.h"

#include <QString>

interface IHashProvider {
    I_DECL(IHashProvider)
    I_NODSCD(QString hashData() const);
};



#endif // IHASHPROVIDER_H
