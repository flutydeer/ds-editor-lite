#ifndef DS_EDITOR_LITE_DOCUMENTIMPORTCONTROLLER_H
#define DS_EDITOR_LITE_DOCUMENTIMPORTCONTROLLER_H

#define documentImportController DocumentImportController::instance()

#include "PreparedImportItem.h"

#include <lite/Core/Singleton.h>

#include <QList>
#include <QObject>
#include <QStringList>
#include <optional>

class DecodeAudioTask;

// Drop target for canvas drops: a stable snapshot of the existing tracks
// starting at the drop row, plus the snapped tick used for audio placement.
// An empty track list means every imported unit gets a brand-new track.
struct FileImportDropTarget {
    QList<int> existingTrackIds;
    int audioStartTick = 0;
};

// Prepares and commits external file imports (canvas drops, the window-level
// drop, and the batch MIDI menu path). Preparation never mutates the model;
// the whole successful batch lands as a single undoable history item.
class DocumentImportController : public QObject {
    Q_OBJECT
public:
    LITE_SINGLETON_DECLARE_INSTANCE(DocumentImportController)

    explicit DocumentImportController(QObject *parent = nullptr);

    // Routes and imports a batch of external files. A lone .dspx goes through
    // the project open flow; mixing a project with other files rejects the
    // whole batch. MIDI and audio are prepared in input order, then committed
    // together (single history item) with per-file failures reported at the
    // end.
    void requestImport(const QStringList &paths,
                       std::optional<FileImportDropTarget> target = std::nullopt);

private:
    void startPreparation();
    void prepareNext();
    void onAudioTaskFinished(DecodeAudioTask *task);
    void onAllPrepared();
    void commitBatch();
    void showSummary() const;
    void reset();
    void showMessageDialog(const QString &title, const QString &message) const;

    QStringList m_pendingPaths;
    QList<PreparedImportItem> m_prepared;
    DecodeAudioTask *m_currentTask = nullptr;
    std::optional<FileImportDropTarget> m_target;
    QByteArray m_codec;
    bool m_importTempo = false;
    bool m_importTimeSignature = false;
    bool m_canceled = false;
    int m_successCount = 0;
    QStringList m_failedMessages;
};

#endif // DS_EDITOR_LITE_DOCUMENTIMPORTCONTROLLER_H
