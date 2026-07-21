//
// Created by fluty on 24-8-2.
//

#ifndef LANGUAGECOMBOBOX_H
#define LANGUAGECOMBOBOX_H

#include "UI/Controls/ComboBox.h"

class LanguageInfo;
class QEvent;

class LanguageComboBox : public ComboBox {
    Q_OBJECT

public:
    explicit LanguageComboBox(const QString &langKey, bool scrollWheelChangeSelection = false,
                              QWidget *parent = nullptr);

    [[nodiscard]] QString currentLanguage() const;
    void setCurrentLanguage(const QString &language);

    QString setLanguages(const QList<LanguageInfo> &languages, const QString &currentLanguage,
                         const QString &preferredLanguage = {});
    QString setLanguageCodes(const QStringList &languageCodes, const QString &currentLanguage,
                             bool preserveUnknownCurrent = true);

signals:
    void currentLanguageChanged(const QString &language);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshDisplayNames();
};

#endif // LANGUAGECOMBOBOX_H
