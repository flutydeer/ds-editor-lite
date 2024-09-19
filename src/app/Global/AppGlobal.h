//
// Created by fluty on 2024/2/6.
//

#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QString>

namespace AppGlobal {
    // Global Constants
    constexpr int resizeTolerance = 8;

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    const QStringList languageKeys = {"cmn", "en", "ja", "unknown"};

    enum LanguageType { cmn, en, ja, unknown };

    enum ParamType {};

    // Global Methods
    QString languageKeyFromType(LanguageType type);
    LanguageType languageTypeFromKey(const QString &languageKey);

    inline QString languageKeyFromType(LanguageType type) {
        return languageKeys[type];
    }

    inline LanguageType languageTypeFromKey(const QString &languageKey) {
        for (int i = 0; i < languageKeys.count(); i++) {
            if (languageKeys[i] == languageKey)
                return static_cast<LanguageType>(i);
        }
        return unknown;
    }
}

#endif // APPGLOBAL_H