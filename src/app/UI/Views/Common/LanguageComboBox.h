#ifndef LANGUAGECOMBOBOX_H
#define LANGUAGECOMBOBOX_H

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/ProjectModel/Voice/LanguageInfo.h>

#include <QIcon>

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
    /// Fills the combo with the "follow singer" entry plus the conversion routes
    /// the singer can actually address. A non-empty `currentLanguage` outside
    /// that set stays selected and flagged as unavailable instead of being
    /// silently replaced.
    QString setConversionLanguages(const QStringList &languageIds, const QString &currentLanguage);
    /// True when the selected entry is an unavailable language kept for display.
    [[nodiscard]] bool currentLanguageUnavailable() const;

signals:
    void currentLanguageChanged(const QString &language);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshDisplayNames();
    void adjustWidthToContent();
    [[nodiscard]] QIcon unavailableIcon() const;

    QList<LanguageInfo> m_languages;
};

#endif // LANGUAGECOMBOBOX_H
