//
// Created by fluty on 24-8-14.
//

#ifndef LANGUAGENAMEUTILS_H
#define LANGUAGENAMEUTILS_H

#define langNameUtils LanguageNameUtils::instance()

#include "Global/AppGlobal.h"
#include "Utils/Singleton.h"

#include <QObject>

class LanguageNameUtils : public QObject, public Singleton<LanguageNameUtils>{
    Q_OBJECT

public:
    const QStringList &names() const {
        return m_languageNames;
    }
    QString name(AppGlobal::LanguageType type) const {
        return m_languageNames[type];
    }

private:
    const QStringList m_languageNames = {tr("Mandarin"), tr("English"), tr("Japanese"),
                                         tr("Unknown")};
};



#endif // LANGUAGENAMEUTILS_H
