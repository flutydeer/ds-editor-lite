#include "NewNoteDefaults.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/GeneralOption.h"

#include <lite/ProjectModel/Voice/LanguageCompatibility.h>

NewNoteDefaults::Defaults NewNoteDefaults::resolve(const SingerInfo &singer,
                                                   const QString &preferredLanguage,
                                                   const GeneralOption &option) {
    Defaults result;

    // The preference is a suggestion inside the singer's capability. When the
    // singer cannot route it, the note stays unspecified instead of carrying a
    // declaration that can never be honored.
    const auto support = LanguageCompatibility::conversionSupport(singer);
    if (support.isKnown() && support.languageIds.contains(preferredLanguage))
        result.language = preferredLanguage;

    const auto lyricLanguage = result.language.isEmpty()
                                   ? LanguageCompatibility::fallbackLanguage(singer)
                                   : result.language;
    result.lyric = option.defaultLyricForLanguage(lyricLanguage);
    return result;
}

NewNoteDefaults::Defaults NewNoteDefaults::resolve(const SingerInfo &singer) {
    const auto general = appOptions->general();
    return resolve(singer, general->defaultSingingLanguage, *general);
}
