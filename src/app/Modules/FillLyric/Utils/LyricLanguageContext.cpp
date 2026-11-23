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
