//
// Created by fluty on 2024/1/27.
//

#ifndef PARAMS_H
#define PARAMS_H

#include "Curve.h"

class Param {
public:
    enum Type { Original, Edited, Envelope, Unknown };

    Param() = default;
    Param(const Param &) = default;
    ~Param();
    [[nodiscard]] const QList<Curve *> &curves(Type type) const;
    void setCurves(Type type, const QList<Curve *> &curves);

    Param &operator=(const Param &from) = default;

    Param &operator=(Param &&from) {
        m_original = from.m_original;
        m_edited = from.m_edited;
        m_envelope = from.m_envelope;
        m_unknown = from.m_unknown;

        from.m_original.clear();
        from.m_edited.clear();
        from.m_envelope.clear();
        from.m_unknown.clear();

        return *this;
    }

private:
    QList<Curve *> m_original;
    QList<Curve *> m_edited;
    QList<Curve *> m_envelope;
    QList<Curve *> m_unknown;
};

class ParamInfo {
public:
    enum Name {
        Pitch,
        Breathiness,
        Tension,
        Velocity,
        Voicing,
        Expressiveness,
        Gender,
        Energy,
        Unknown
    };

    Param pitch;
    Param breathiness;
    Param tension;
    Param velocity;
    Param voicing;
    Param expressiveness;
    Param gender;
    Param energy;

    Param *getParamByName(Name name);
};



#endif // PARAMS_H
