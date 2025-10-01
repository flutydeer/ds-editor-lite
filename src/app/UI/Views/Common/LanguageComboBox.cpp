//
// Created by fluty on 24-8-2.
//

#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"

LanguageComboBox::LanguageComboBox(const QString &langKey, bool scrollWheelChangeSelection,
                                   QWidget *parent)
    : ComboBox(scrollWheelChangeSelection, parent) {
    addItems(AppGlobal::languageNames);
    setCurrentText(langKey);
}