#ifndef DS_EDITOR_LITE_MIDILOADSESSION_H
#define DS_EDITOR_LITE_MIDILOADSESSION_H

#include "IProjectLoadSession.h"

#include "Modules/ProjectConverters/MidiConverterUi.h"
#include "Modules/ProjectFormats/UserInput.h"

class IProjectFormatHandler;
class MidiParseTask;
class MidiReprocessTask;
class MidiConfigPage;
class ProjectImportConfigDialog;

// MIDI load session split into background parse, interactive configuration
// and materialization. Replaces the legacy synchronous MidiConverter flow
// while keeping the same user-visible behaviour: Open always replaces,
// Import always appends, tempo / time signature default to enabled on Open
// and disabled on Import. Channel toggles in the configuration dialog are
// re-processed in the background; stale results are dropped by generation id.
class MidiLoadSession final : public IProjectLoadSession {
    Q_OBJECT

public:
    MidiLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                    ProjectLoadPurpose purpose, quint64 requestId, QObject *parent = nullptr);
    ~MidiLoadSession() override;

    void start() override;
    void cancel() override;
    PreparedProject takeResult() override;
    [[nodiscard]] quint64 requestId() const override;

private:
    void startParseTask();
    void handleTaskFinished(MidiParseTask *task);
    void startConfiguration();
    void requestReprocess(bool separateChannels);
    void handleReprocessFinished(quint64 generation, MidiReprocessTask *task);
    void materialize(const MidiUserInput &input);
    void detachTask();
    void detachReprocessTask();

    IProjectFormatHandler *m_formatHandler = nullptr;
    QString m_filePath;
    ProjectLoadPurpose m_purpose = ProjectLoadPurpose::Open;
    quint64 m_requestId = 0;
    MidiParseTask *m_task = nullptr;
    MidiReprocessTask *m_reprocessTask = nullptr;
    quint64 m_reprocessGeneration = 0;
    ProjectImportConfigDialog *m_dialog = nullptr;
    MidiConfigPage *m_configPage = nullptr;
    MidiConverterUi m_converterUi;
    MidiParseData m_parseData;
    PreparedProject m_result;
    bool m_started = false;
    bool m_terminal = false;
};

#endif // DS_EDITOR_LITE_MIDILOADSESSION_H
