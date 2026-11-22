#ifndef LANGUAGEREPAIR_H
#define LANGUAGEREPAIR_H

namespace Automation {
    /// What a voice-context command does with note language declarations the
    /// target singer cannot synthesize.
    enum class LanguageRepair {
        /// Keep every declaration. Conflicts stay visible as diagnostics.
        None,
        /// Redeclare unsupported notes with the target's default conversion
        /// language, so the notes can be synthesized again.
        MapUnsupportedToDefault,
    };
}

#endif // LANGUAGEREPAIR_H
