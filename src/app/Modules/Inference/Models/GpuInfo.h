//
// Created by fluty on 24-9-28.
//

#ifndef GPUINFO_H
#define GPUINFO_H

#include <QString>

class GpuInfo {
public:
    int index = -1;
    QString description;
    unsigned long long memory;
};

#endif //GPUINFO_H
