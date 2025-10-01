//
// Created by fluty on 24-8-2.
//

#ifndef LANGUAGECOMBOBOX_H
#define LANGUAGECOMBOBOX_H

#include "UI/Controls/ComboBox.h"

class LanguageComboBox : public ComboBox {
    Q_OBJECT

public:
    explicit LanguageComboBox(const QString &langKey, bool scrollWheelChangeSelection = false,
                              QWidget *parent = nullptr);
};

#endif // LANGUAGECOMBOBOX_H
