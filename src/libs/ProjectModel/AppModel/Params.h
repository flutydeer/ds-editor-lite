#ifndef PARAMS_H
#define PARAMS_H

#include <QList>

#include <lite/ProjectModel/AppModel/Curve.h>

class Param {
public:
    enum Type { Original, Edited, Envelope, Unknown };

    Param() = default;
    Param(const Param &) = default;
    ~Param();
    [[nodiscard]] const QList<Curve *> &curves(Type type) const;
    void setCurves(Type type, const QList<Curve *> &curves, SingingClip *clip);

    Param &operator=(const Param &from) = default;

    Param &operator=(Param &&from) noexcept {
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
        Expressiveness,
        Energy,
        Breathiness,
        Voicing,
        Tension,
        MouthOpening,
        Gender,
        Velocity,
        ToneShift,
        SpeakerMix,
        Unknown
    };

    explicit ParamInfo(SingingClip *clip);

    Param pitch;
    Param expressiveness;
    Param energy;
    Param breathiness;
    Param voicing;
    Param tension;
    Param mouthOpening;
    Param gender;
    Param velocity;
    Param toneShift;

    Param *getParamByName(Name name);
    const Param *getParamByName(Name name) const;
    static bool hasOriginalParam(Name name);
    static bool supportsCurveTransform(Name name);

private:
    QPointer<SingingClip> m_clip;
};



#endif // PARAMS_H
