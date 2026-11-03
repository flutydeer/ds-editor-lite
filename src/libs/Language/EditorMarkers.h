#ifndef LITE_LANGUAGE_EDITORMARKERS_H
#define LITE_LANGUAGE_EDITORMARKERS_H

#include <QString>

namespace Language {
    /// Markers the editor owns rather than the language backend: the slur and the rest marker.
    /// They are syntax, so they must reach the editor unchanged and never be treated as lyrics to
    /// convert. Kept free of any project dependency so the rule can be tested on its own.
    ///
    /// TODO: voicebanks declare their own reserved keys. Once the editor reads that declaration,
    /// this predicate should answer from it as well instead of naming the two markers here.
    inline bool isEditorMarker(const QString &lyric) {
        return lyric == QStringLiteral("-") || lyric == QStringLiteral("AP");
    }
} // namespace Language

#endif // LITE_LANGUAGE_EDITORMARKERS_H
