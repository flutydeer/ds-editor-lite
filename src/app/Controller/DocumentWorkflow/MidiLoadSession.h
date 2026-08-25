#ifndef DS_EDITOR_LITE_MIDILOADSESSION_H
#define DS_EDITOR_LITE_MIDILOADSESSION_H

#include "ProjectLoadSessionBase.h"

#include "Modules/ProjectConverters/MidiConverterUi.h"
#include "Modules/ProjectFormats/UserInput.h"

class IProjectConfigPage;
class IProjectFormatHandler;

// MIDI load session split into background parse, interactive configuration
// and materialization. Replaces the legacy synchronous MidiConverter flow
// while keeping the same user-visible behaviour: Open always replaces,
// Import always appends, tempo / time signature default to enabled on Open
// and disabled on Import. Channel toggles in the configuration dialog are
// re-processed in the background; stale results are dropped by generation id.
class MidiLoadSession final : public ProjectLoadSessionBase {
    Q_OBJECT

public:
    MidiLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                    ProjectLoadPurpose purpose, quint64 requestId, bool interactive = true,
                    QByteArray encoding = {}, bool importTempo = true,
                    bool importTimeSignature = true,
                    QObject *parent = nullptr);

private:
    void onStart() override;
    Task *createParseTask() override;
    void handleParseResult(Task *task) override;
    Task *createReprocessTask() override;
    void handleReprocessResult(Task *task) override;

    void startConfiguration();
    void requestReprocess(bool separateChannels);
    void materialize(const MidiUserInput &input);

    IProjectFormatHandler *m_formatHandler = nullptr;
    ProjectLoadPurpose m_purpose = ProjectLoadPurpose::Open;
    bool m_separateChannels = false;
    IProjectConfigPage *m_configPage = nullptr;
    MidiConverterUi m_converterUi;
    MidiParseData m_parseData;
    bool m_interactive = true;
    QByteArray m_encoding;
    bool m_importTempo = true;
    bool m_importTimeSignature = true;
};

#endif // DS_EDITOR_LITE_MIDILOADSESSION_H
