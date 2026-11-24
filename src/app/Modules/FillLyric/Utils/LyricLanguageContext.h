#ifndef LYRICLANGUAGECONTEXT_H
#define LYRICLANGUAGECONTEXT_H

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace FillLyric {

    /// One language decision shared by the whole fill-lyric chain.
    ///
    /// Built once from the clip's singer and the languages the notes being
    /// filled already declare, then handed to the splitter, the tagger, G2P and
    /// the write-back so every step ranks and resolves languages the same way.
    /// The choice is a priority: it never forces a token whose language is
    /// already known and routable, and the selector only offers routable
    /// languages, so a choice is either empty or convertible.
    class LyricLanguageContext {
    public:
        /// Re-derives the initial choice for a new skip-slur state. Empty skips
        /// the derivation, leaving the choice as given.
        using ChoiceResolver = std::function<QString(bool skipSlur)>;

        LyricLanguageContext() = default;
        LyricLanguageContext(QStringList supportedLanguages, QString fallbackLanguage,
                             QString choice, ChoiceResolver choiceResolver = {});

        /// Addressable conversion routes of the singer, in route order.
        [[nodiscard]] const QStringList &supportedLanguages() const;
        /// Route an unspecified note resolves to.
        [[nodiscard]] const QString &fallbackLanguage() const;
        /// The selected language. Empty means automatic detection.
        [[nodiscard]] const QString &choice() const;
        /// Records a selection made in the language control, which the derived
        /// choice then never overwrites.
        void setChoice(const QString &choice);
        /// Restores a previously shown choice together with its origin, so a
        /// canceled change leaves a derived choice derived.
        void restoreChoice(const QString &choice, bool explicitChoice);
        /// True once the current choice came from the language control.
        [[nodiscard]] bool choiceIsExplicit() const;

        /// Re-derives the choice for a new skip-slur state and reports whether it
        /// changed. A derived choice follows the notes being filled, so skipping
        /// slurs can drop a language from the vote. A selection the user made
        /// stays untouched.
        bool refreshChoice(bool skipSlur);

        /// Tagger priority: the choice first, then the singer's route order.
        [[nodiscard]] QStringList priorityLanguages() const;
        /// Languages the selector offers.
        [[nodiscard]] QStringList selectableLanguages() const;

        /// The stored note declaration for a language the tagger detected.
        /// Returns empty for an unspecified note.
        [[nodiscard]] QString resolvedNoteLanguage(const QString &taggedLanguage) const;

        [[nodiscard]] bool operator==(const LyricLanguageContext &other) const;
        [[nodiscard]] bool operator!=(const LyricLanguageContext &other) const;

    private:
        QStringList m_supported;
        QString m_fallback;
        QString m_choice;
        ChoiceResolver m_choiceResolver;
        bool m_choiceIsExplicit = false;
    };

    using LyricLanguageContextPtr = std::shared_ptr<LyricLanguageContext>;

}

#endif // LYRICLANGUAGECONTEXT_H
