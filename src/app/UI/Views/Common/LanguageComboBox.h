#ifndef LANGUAGECOMBOBOX_H
#define LANGUAGECOMBOBOX_H

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/ProjectModel/Voice/LanguageInfo.h>

class LanguageInfo;
class QEvent;

class LanguageComboBox : public ComboBox {
    Q_OBJECT

public:
    explicit LanguageComboBox(const QString &langKey,
                              WheelEventPolicy wheelEventPolicy = WheelEventPolicy::Consume,
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
    void adjustWidthToContent();

    QList<LanguageInfo> m_languages;
};

#endif // LANGUAGECOMBOBOX_H
