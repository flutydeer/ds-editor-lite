#ifndef DIFFSCOPEAUDIOWORKSPACE_H
#define DIFFSCOPEAUDIOWORKSPACE_H

#include <QJsonObject>
#include <QMap>

struct AudioPathInfo;

// Reads/writes audio locating info in the "diffscope" -> "audio" namespace of the clip workspace.
// This namespace is shared with the DiffScope editor: fields absoluteDir / relativeDir / fileName / sha512
// are read and written by both editors; fields DiffScope additionally writes (formatEntryClassName / userData, etc.)
// must be preserved as-is (round-trip), so writes only merge known subkeys and never rebuild the object.
namespace DiffscopeAudioWorkspace {

    AudioPathInfo read(const QMap<QString, QJsonObject> &workspace);

    void write(QMap<QString, QJsonObject> &workspace, const AudioPathInfo &info,
               const QString &absolutePath);

    // Computes the audio file directory's path relative to the project file directory.
    // Returns an empty string if the audio is outside the project directory (or its subdirectories) or either path is empty;
    // returns "." when in the same directory as the project file (matches DiffScope behavior)
    QString relativeDirFor(const QString &audioFilePath, const QString &projectFilePath);

} // namespace DiffscopeAudioWorkspace

#endif // DIFFSCOPEAUDIOWORKSPACE_H
