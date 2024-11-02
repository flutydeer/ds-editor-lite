//
// Created by fluty on 24-8-14.
//

#ifndef LANGUAGENAMEUTILS_H
#define LANGUAGENAMEUTILS_H

#define langNameUtils LanguageNameUtils::instance()

#include "Global/AppGlobal.h"
#include "Utils/Singleton.h"

#include <QObject>

class LanguageNameUtils : public QObject, public Singleton<LanguageNameUtils> {
    Q_OBJECT

public:
    static const QStringList &names() {
        return AppGlobal::languageNames;
    }
};



#endif // LANGUAGENAMEUTILS_H
