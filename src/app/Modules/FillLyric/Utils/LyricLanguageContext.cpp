#include "Modules/FillLyric/Utils/LyricLanguageContext.h"

#include <utility>

namespace FillLyric {
    LyricLanguageContext::LyricLanguageContext(QStringList supportedLanguages,
                                               QString fallbackLanguage, QString choice,
                                               ChoiceResolver choiceResolver)
        : m_supported(std::move(supportedLanguages)), m_fallback(std::move(fallbackLanguage)),
          m_choice(std::move(choice)), m_choiceResolver(std::move(choiceResolver)) {
    }

    const QStringList &LyricLanguageContext::supportedLanguages() const {
        return m_supported;
    }

    const QString &LyricLanguageContext::fallbackLanguage() const {
        return m_fallback;
    }

    const QString &LyricLanguageContext::choice() const {
        return m_choice;
    }

    void LyricLanguageContext::setChoice(const QString &choice) {
        m_choice = choice;
        m_choiceIsExplicit = true;
    }

    void LyricLanguageContext::restoreChoice(const QString &choice, const bool explicitChoice) {
        m_choice = choice;
        m_choiceIsExplicit = explicitChoice;
    }

    bool LyricLanguageContext::choiceIsExplicit() const {
        return m_choiceIsExplicit;
    }

    bool LyricLanguageContext::refreshChoice(const bool skipSlur) {
        if (m_choiceIsExplicit || !m_choiceResolver)
            return false;
        auto derived = m_choiceResolver(skipSlur);
        if (derived == m_choice)
            return false;
        m_choice = std::move(derived);
        return true;
    }

    QStringList LyricLanguageContext::priorityLanguages() const {
        QStringList result;
        const auto appendUnique = [&result](const QString &language) {
            if (!language.isEmpty() && !result.contains(language))
                result.append(language);
        };
        appendUnique(m_choice);
        appendUnique(m_fallback);
        for (const auto &language : m_supported)
            appendUnique(language);
        return result;
    }

    QStringList LyricLanguageContext::selectableLanguages() const {
        return m_supported;
    }

    QString LyricLanguageContext::resolvedNoteLanguage(const QString &taggedLanguage) const {
        auto language = taggedLanguage;
        if (language == QLatin1String("unknown"))
            language.clear();
        // The choice only catches tokens the tagger could not identify or route.
        if (!m_choice.isEmpty() && (language.isEmpty() || !m_supported.contains(language)))
            language = m_choice;
        return language;
    }

    bool LyricLanguageContext::operator==(const LyricLanguageContext &other) const {
        return m_supported == other.m_supported && m_fallback == other.m_fallback &&
               m_choice == other.m_choice;
    }

    bool LyricLanguageContext::operator!=(const LyricLanguageContext &other) const {
        return !(*this == other);
    }

}
