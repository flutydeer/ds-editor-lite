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

    CurveType type() const override {
        return Draw;
    }
    int step = 5;
    [[nodiscard]] const QList<int> &values() const;
    void setValues(const QList<int> &values);
    void insertValue(int index, int value);
    void insertValues(int index, const QList<int> &values);
    void removeValueRange(int i, int n);
    void clearValues();
    void appendValue(int value);
    void replaceValue(int index, int value);
    void mergeWith(const DrawCurve &other);
    void overlayMergeWith(const DrawCurve &other);
    void eraseWith(const DrawCurve &other);

    [[nodiscard]] int endTick() const override;

private:
    // int m_step = 5;
    QList<int> m_values;
};



#endif // DRAWCURVE_H
