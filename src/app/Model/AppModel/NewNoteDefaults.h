#ifndef NEWNOTEDEFAULTS_H
#define NEWNOTEDEFAULTS_H

#include <lite/ProjectModel/Voice/SingerInfo.h>

#include <QString>

class GeneralOption;

/// Creation policy for interactively created notes.
///
/// The language marker and the placeholder lyric come from the same decision,
/// so a note is never given a lyric looked up for a different language. The
/// application preference only applies when the singer can route it, and
/// nothing here is applied when a note comes from import or automation.
namespace NewNoteDefaults {

    struct Defaults {
        /// Stored declaration. Empty means the note stays unspecified.
        QString language;
        /// Placeholder lyric matching the language the note will actually use.
        QString lyric;
    };

    /// Pure policy: no side effects, no dependency on the current document.
    Defaults resolve(const SingerInfo &singer, const QString &preferredLanguage,
                     const GeneralOption &option);

    /// Same policy under the current application settings.
    Defaults resolve(const SingerInfo &singer);

}

#endif // NEWNOTEDEFAULTS_H
