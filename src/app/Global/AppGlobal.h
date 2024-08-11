//
// Created by fluty on 2024/2/6.
//

#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QColor>

namespace AppGlobal {
    const int horizontalScrollBarWidth = 16;
    const int verticalScrollBarWidth = 16;

    const QColor overlappedViewBorder = QColor(255, 80, 80);
    const int resizeTolarance = 8;

    const QColor primaryColor = QColor(155, 186, 255);
    const QColor primaryColorHover = QColor(169, 196, 255);
    const QColor primaryColorPressed = QColor(139, 175, 255);
    const QColor primaryColorDisabled = QColor(205, 205, 205);

    const QColor primaryforegroundColorNormal = QColor(0, 28, 92);
    const QColor primaryforegroundColorHover = QColor(0, 28, 92);
    const QColor primaryforegroundColorPressed = QColor(0, 28, 92);
    const QColor primaryforegroundColorDisabled = QColor(166, 166, 166);

    const QColor controlBackgroundColorNormal = QColor(64, 64, 64);
    const QColor controlBackgroundColorHover = QColor(72, 72, 72);
    const QColor controlBackgroundColorPressed = QColor(56, 56, 56);
    const QColor controlBackgroundColorDisabled = QColor(64, 64, 64);

    const QColor controlForegroundColorNormal = QColor(242, 242, 242);
    const QColor controlForegroundColorHover = QColor(242, 242, 242);
    const QColor controlForegroundColorPressed = QColor(191, 191, 191);
    const QColor controlForegroundColorDisabled = QColor(127, 127, 127);

    const QColor controlBorderColorNormal = QColor(89, 89, 89);
    const QColor controlBorderColorHover = QColor(89, 89, 89);
    const QColor controlBorderColorPressed = QColor(63, 63, 63);
    const QColor controlBorderColorDisabled = QColor(63, 63, 63);

    const QColor barLineColor = QColor(92, 96, 100);
    const QColor barTextColor = QColor(200, 200, 200);
    const QColor backgroundColor = QColor(42, 43, 44);
    const QColor beatLineColor = QColor(72, 75, 78);
    const QColor commonLineColor = QColor(57, 59, 61);
    const QColor beatTextColor = QColor(160, 160, 160);

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    const QStringList languageKeys = {"cmn", "en", "ja", "unknown"};

    enum LanguageType { cmn, en, ja, unknown };

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