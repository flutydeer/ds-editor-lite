//
// Created by fluty on 24-8-2.
//

#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"
#include "UI/Utils/LanguageNameUtils.h"

LanguageComboBox::LanguageComboBox(const QString &langKey, bool scrollWheelChangeSelection,
                                   QWidget *parent)
    : ComboBox(scrollWheelChangeSelection, parent) {
    auto languageType = AppGlobal::languageTypeFromKey(langKey);
    addItems(langNameUtils->names());
    setCurrentIndex(languageType);
}