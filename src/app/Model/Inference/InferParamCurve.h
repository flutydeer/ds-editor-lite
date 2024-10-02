//
// Created by fluty on 24-10-2.
//

#ifndef INFERPARAMCURVE_H
#define INFERPARAMCURVE_H

#include <QList>

class InferParamCurve {

public:
    // const int step = 5;
    QList<double> values;

    friend bool operator==(const InferParamCurve &lhs, const InferParamCurve &rhs) {
        return lhs.values == rhs.values;
    }

    friend bool operator!=(const InferParamCurve &lhs, const InferParamCurve &rhs) {
        return !(lhs == rhs);
    }
};

#endif // INFERPARAMCURVE_H
