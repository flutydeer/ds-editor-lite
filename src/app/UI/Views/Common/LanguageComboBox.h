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

    // A language the singer does not serve is normally left out of the list. Naming it in
    // `unavailable` shows it disabled instead, which is what tells a clip whose own language the
    // singer cannot sing why it is being sung in another one.
    QString setLanguages(const QList<LanguageInfo> &languages, const QString &currentLanguage,
                         const QString &preferredLanguage = {},
                         const QStringList &unavailable = {});
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
