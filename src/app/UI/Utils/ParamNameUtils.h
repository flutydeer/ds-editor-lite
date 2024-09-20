//
// Created by fluty on 24-9-19.
//

#ifndef PARAMNAMEUTILS_H
#define PARAMNAMEUTILS_H

#define paramNameUtils ParamNameUtils::instance()

#include "Model/AppModel/Params.h"
#include "Utils/Singleton.h"

#include <QObject>

class ParamNameUtils : public QObject, public Singleton<ParamNameUtils> {
    Q_OBJECT
public:
    [[nodiscard]] const QStringList &names() const {
        return m_names;
    }

    [[nodiscard]] QString nameFromType(ParamInfo::Name name) const {
        return m_names[name];
    }

private:
    const QStringList m_names = {tr("Pitch"),   tr("Breathiness"),    tr("Tension"), tr("Velocity"),
                                 tr("Voicing"), tr("Expressiveness"), tr("Gender"),  tr("Energy")};
    const QStringList m_keys = {"pitch",   "breathiness",    "tension", "velocity",
                                "voicing", "expressiveness", "gender",  "energy"};
};

#endif // PARAMNAMEUTILS_H
