#ifndef LYRICLANGUAGECONTEXT_H
#define LYRICLANGUAGECONTEXT_H

#include <QString>
#include <QStringList>

#include <memory>

namespace FillLyric {

    /// One language decision shared by the whole fill-lyric chain.
    ///
    /// Built once from the clip's singer and its stored input language, then
    /// handed to the splitter, the tagger, G2P and the write-back so every step
    /// ranks and resolves languages the same way. The choice is a priority: it
    /// never forces a token whose language is already known and routable.
    class LyricLanguageContext {
    public:
        LyricLanguageContext() = default;
        LyricLanguageContext(QStringList supportedLanguages, QString fallbackLanguage,
                             QString choice);

        /// Addressable conversion routes of the singer, in route order.
        [[nodiscard]] const QStringList &supportedLanguages() const;
        /// Route an unspecified note resolves to.
        [[nodiscard]] const QString &fallbackLanguage() const;
        /// The selected language. Empty means automatic detection.
        [[nodiscard]] const QString &choice() const;
        void setChoice(const QString &choice);

        /// Tagger priority: the choice first, then the singer's route order.
        [[nodiscard]] QStringList priorityLanguages() const;
        /// Languages the selector offers.
        [[nodiscard]] QStringList selectableLanguages() const;
        /// True when a choice is set but the singer cannot route it.
        [[nodiscard]] bool choiceUnavailable() const;

        /// The stored note declaration for a language the tagger detected.
        /// Returns empty for an unspecified note.
        [[nodiscard]] QString resolvedNoteLanguage(const QString &taggedLanguage) const;

        [[nodiscard]] bool operator==(const LyricLanguageContext &other) const;
        [[nodiscard]] bool operator!=(const LyricLanguageContext &other) const;

    private:
        QStringList m_supported;
        QString m_fallback;
        QString m_choice;
    };

    using LyricLanguageContextPtr = std::shared_ptr<LyricLanguageContext>;

}

#endif // LYRICLANGUAGECONTEXT_H
