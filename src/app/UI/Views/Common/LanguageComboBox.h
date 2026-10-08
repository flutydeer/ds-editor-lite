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

    // The language codes in `unavailable` are always rendered as greyed-out items that cannot be
    // clicked, rather than left out of the list, and cover two cases:
    //   a) the host knows the language, but this singer does not declare it — if the clip itself
    //      uses it, the UI has to state which one is lost;
    //   b) the singer declares the language, but its G2P cannot be converted (a failure on the
    //      engine side) — only this one is disabled, the other languages stay selectable.
    // A disabled code that is declared in `languages` keeps the package's localized name, an
    // undeclared one uses the generic name.
    // `unavailableReasons` gives the original reason text per language code (an entry may be
    // missing or empty): a non-empty original text is copied verbatim — only the loader/engine
    // knows why it refused, the same convention as SingerMenuUnavailablePackages — otherwise a
    // translated fallback wording is used. Whether an item is disabled is decided by `unavailable`
    // alone, and the reason table only supplies text. When the current language is itself a
    // disabled one, this widget's tooltip is temporarily changed to that reason too; switching
    // back to an available language restores the tooltip the caller set, and in the normal state
    // this widget never touches the tooltip.
    QString setLanguages(const QList<LanguageInfo> &languages, const QString &currentLanguage,
                         const QString &preferredLanguage = {},
                         const QStringList &unavailable = {},
                         const QHash<QString, QString> &unavailableReasons = {});
    QString setLanguageCodes(const QStringList &languageCodes, const QString &currentLanguage,
                             bool preserveUnknownCurrent = true);

    // Computes which languages "should be disabled" for a singer, for setLanguages: a) as above
    // (the host knows it but the singer does not declare it, no reason is available); b) the
    // singer declares it, but the engine reports the code as not convertible, and the reasons
    // are collected into \a reasons. The returned list is ordered (host list first, package
    // declaration order after). Only a resolved (ResolutionState::Resolved) singer is queried
    // against the engine: the engine has no answer for an unresolved singer, and the query
    // would only produce noise. This function loads no model and no package.
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
