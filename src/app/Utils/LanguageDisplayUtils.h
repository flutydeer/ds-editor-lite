#ifndef LANGUAGEDISPLAYUTILS_H
#define LANGUAGEDISPLAYUTILS_H

#include <QString>

// Localized labels for conversion language ids. The ids come from the voicebank
// package; the built-in ones get a translated name so the UI never shows a bare
// ISO code, and unknown ids fall back to the id itself.
namespace LanguageDisplayUtils {
    [[nodiscard]] QString displayName(const QString &languageId);
}

#endif // LANGUAGEDISPLAYUTILS_H
