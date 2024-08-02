//
// Created by fluty on 24-8-2.
//

#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"

LanguageComboBox::LanguageComboBox(const QString &langKey, QWidget *parent) : ComboBox(parent) {
    auto languageType = AppGlobal::languageTypeFromKey(langKey);
    const QStringList languageNames = {tr("Mandarin"), tr("English"), tr("Japanese"),
                                       tr("Unknown")};
    addItems(languageNames);
    setCurrentIndex(languageType);
}