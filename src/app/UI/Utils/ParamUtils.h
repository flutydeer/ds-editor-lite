//
// Created by fluty on 24-8-14.
//

#ifndef PARAMUTILS_H
#define PARAMUTILS_H

#include <QList>

class ParamUtils {
public:
    static QList<std::pair<int, int>> loadOpensvipPitchParam(const QString &filename);
};



#endif // PARAMUTILS_H
