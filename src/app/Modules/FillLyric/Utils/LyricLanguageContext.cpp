#include "Modules/FillLyric/Utils/LyricLanguageContext.h"

#include <utility>

namespace FillLyric {
    LyricLanguageContext::LyricLanguageContext(QStringList supportedLanguages,
                                               QString fallbackLanguage, QString choice)
        : m_supported(std::move(supportedLanguages)), m_fallback(std::move(fallbackLanguage)),
          m_choice(std::move(choice)) {
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
    }

    QStringList LyricLanguageContext::priorityLanguages() const {
        QStringList result;
        const auto appendUnique = [&result](const QString &language) {
            if (!language.isEmpty() && !result.contains(language))
                result.append(language);
        };
        // A choice the singer cannot route does not take part in resolution, so
        // the dialog warning stays the only place that value can lead.
        if (!choiceUnavailable())
            appendUnique(m_choice);
        appendUnique(m_fallback);
        for (const auto &language : m_supported)
            appendUnique(language);
        return result;
    }

    QStringList LyricLanguageContext::selectableLanguages() const {
        return m_supported;
    }

    bool LyricLanguageContext::choiceUnavailable() const {
        return !m_choice.isEmpty() && !m_supported.contains(m_choice);
    }

    QString LyricLanguageContext::resolvedNoteLanguage(const QString &taggedLanguage) const {
        auto language = taggedLanguage;
        if (language == QLatin1String("unknown"))
            language.clear();
        // The choice only catches tokens the tagger could not identify or route.
        // A choice the singer cannot route never acts as the fallback, because
        // no conversion route exists for it.
        if (!m_choice.isEmpty() && !choiceUnavailable() &&
            (language.isEmpty() || !m_supported.contains(language)))
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
