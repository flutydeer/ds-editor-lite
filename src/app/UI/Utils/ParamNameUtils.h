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
    const QStringList m_names = {tr("Pitch"),       tr("Expressiveness"), tr("Energy"),
                                 tr("Breathiness"), tr("Voicing"),        tr("Tension"),
                                 tr("Gender"),      tr("Velocity")};
    const QStringList m_keys = {"pitch",   "expressiveness", "energy", "breathiness",
                                "voicing", "tension",        "gender", "velocity"};
};

#endif // PARAMNAMEUTILS_H
