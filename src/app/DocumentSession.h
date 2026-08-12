#ifndef DOCUMENTSESSION_H
#define DOCUMENTSESSION_H

#include <QUuid>
#include <QtGlobal>
#include <QObject>

#include <memory>

class AppController;
class AppModel;
class AppStatus;
class AudioContext;
class AudioDecodingController;
class ClipboardController;
class ClipController;
class DocumentImportController;
class DocumentWorkflowController;
class EditorViewController;
class EditSessionManager;
class HistoryManager;
class InferController;
class LevelMeterManager;
class MidiExtractController;
class ParamUtils;
class PitchExtractController;
class PlaybackController;
class ProjectFormatRegistry;
class ProjectPackageResolver;
class ProjectStatusController;
class TrackController;
class UndoRedoController;

class DocumentSession final : public QObject {
public:
    explicit DocumentSession(QObject *parent = nullptr);
    ~DocumentSession() override;

    Q_DISABLE_COPY_MOVE(DocumentSession)

    QUuid id() const;
    void activate();

    DocumentWorkflowController *workflow() const;
    PlaybackController *playback() const;

private:
    friend class AppContext;
    void registerServices();
    void unregisterServices();

    QUuid m_id = QUuid::createUuid();
    AppStatus *m_appStatus = nullptr;
    AppModel *m_appModel = nullptr;
    ParamUtils *m_paramUtils = nullptr;
    HistoryManager *m_historyManager = nullptr;
    LevelMeterManager *m_levelMeterManager = nullptr;
    AudioDecodingController *m_audioDecodingController = nullptr;
    ClipboardController *m_clipboardController = nullptr;
    TrackController *m_trackController = nullptr;
    ClipController *m_clipController = nullptr;
    EditorViewController *m_editorViewController = nullptr;
    UndoRedoController *m_undoRedoController = nullptr;
    PitchExtractController *m_pitchExtractController = nullptr;
    MidiExtractController *m_midiExtractController = nullptr;
    EditSessionManager *m_editSessionManager = nullptr;
    PlaybackController *m_playbackController = nullptr;
    ProjectStatusController *m_projectStatusController = nullptr;
    ProjectPackageResolver *m_projectPackageResolver = nullptr;
    InferController *m_inferController = nullptr;
    ProjectFormatRegistry *m_projectFormatRegistry = nullptr;
    DocumentImportController *m_documentImportController = nullptr;
    std::unique_ptr<AudioContext> m_audioContext;
    AppController *m_appController = nullptr;
    DocumentWorkflowController *m_documentWorkflowController = nullptr;
};

#endif // DOCUMENTSESSION_H
