#ifndef DS_EDITOR_LITE_AUDIOFILEPREPARER_H
#define DS_EDITOR_LITE_AUDIOFILEPREPARER_H

#include "PreparedImportItem.h"

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace talcs {
    class AbstractAudioFormatIO;
}

class DecodeAudioTask;

// Prepares audio files for import without mutating AppModel or the history
// stack. Shared by the file-dialog path and the drag-and-drop path so both go
// through the same decode pipeline.
class AudioFilePreparer {
public:
    // Builds the decode task for `path`. `io` and `workspace` may be supplied
    // by the caller when a file dialog already probed the format; when absent
    // the preparer probes the format itself via the shared FormatManager and
    // synthesizes the workspace from the matched format entry. The returned
    // task is owned by the caller (connect `finished`, then delete).
    static DecodeAudioTask *createPrepareTask(const QString &path,
                                              talcs::AbstractAudioFormatIO *io = nullptr,
                                              const QJsonObject &workspace = {});

    // Extracts the prepared item from a successfully finished task.
    static PreparedAudioItem prepareResult(const DecodeAudioTask *task);

    // Packs format data into the workspace JSON shape the model expects.
    static QJsonObject makeWorkspace(const QVariant &userData, const QString &entryClassName);

private:
    static QJsonObject probeWorkspace(const QString &path);
};

#endif // DS_EDITOR_LITE_AUDIOFILEPREPARER_H
