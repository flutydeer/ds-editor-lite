#ifndef LANGUAGECOMBOBOX_H
#define LANGUAGECOMBOBOX_H

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/ProjectModel/Voice/LanguageInfo.h>

#include <QHash>

class LanguageInfo;
class QEvent;
class SingerInfo;

class LanguageComboBox : public ComboBox {
    Q_OBJECT

public:
    explicit LanguageComboBox(const QString &langKey,
                              WheelEventPolicy wheelEventPolicy = WheelEventPolicy::Consume,
                              QWidget *parent = nullptr);

    [[nodiscard]] QString currentLanguage() const;
    void setCurrentLanguage(const QString &language);

    // List unavailable languages as disabled while preserving their names and any current
    // selection.
    // Show the engine's reason or translated fallback text. A disabled selection also overrides
    // this widget's tooltip until an available language is selected.
    QString setLanguages(const QList<LanguageInfo> &languages, const QString &currentLanguage,
                         const QString &preferredLanguage = {},
                         const QStringList &unavailable = {},
                         const QHash<QString, QString> &unavailableReasons = {});
    QString setLanguageCodes(const QStringList &languageCodes, const QString &currentLanguage,
                             bool preserveUnknownCurrent = true);

    // Return undeclared or unconvertible languages, with available failure reasons.
    // Preserve host order followed by singer declaration order; only probe resolved singers.
    static QStringList unavailableLanguages(const SingerInfo &singerInfo,
                                            QHash<QString, QString> &reasons);

signals:
    void currentLanguageChanged(const QString &language);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshDisplayNames();
    void adjustWidthToContent();
    void markUnavailableItem(int index, const QString &reason);
    void refreshCurrentToolTip();

    QList<LanguageInfo> m_languages;
    QString m_baseToolTip;   // caller's ordinary tooltip; restored after a disabled item used it
    QString m_reasonToolTip; // the reason tooltip this widget writes; empty = not taken over yet
};

#endif // LANGUAGECOMBOBOX_H
