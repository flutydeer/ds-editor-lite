//
// Created by fluty on 24-8-14.
//

#ifndef DRAWCURVE_H
#define DRAWCURVE_H

#include "Curve.h"

class DrawCurve final : public Curve {
public:
    DrawCurve() = default;
    explicit DrawCurve(int id) : Curve(id){};
    DrawCurve(const DrawCurve &other);

    [[nodiscard]] CurveType type() const override {
        return Draw;
    }

    int step = 5;
    [[nodiscard]] const QList<int> &values() const;
    [[nodiscard]] QList<int> mid(int tick) const;
    void setValues(const QList<int> &values);
    void insertValue(int index, int value);
    void insertValues(int index, const QList<int> &values);
    void removeValueRange(qsizetype i, qsizetype n);
    void clearValues();
    void appendValue(int value);
    void replaceValue(int index, int value);
    void mergeWithCurrentPriority(const DrawCurve &other);
    void mergeWithOtherPriority(const DrawCurve &other);
    void erase(int otherStart, int otherEnd);
    void eraseTail(int length);
    void eraseTailFrom(int tick);

    [[nodiscard]] int endTick() const override;

private:
    // int m_step = 5;
    QList<int> m_values;
};



#endif // DRAWCURVE_H
