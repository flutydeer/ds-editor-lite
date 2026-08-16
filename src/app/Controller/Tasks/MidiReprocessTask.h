#ifndef DS_EDITOR_LITE_MIDIREPROCESSTASK_H
#define DS_EDITOR_LITE_MIDIREPROCESSTASK_H

#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/Tasking/Task.h>

#include <opendspx/converter/midi/midiintermediatedata.h>

#include <QByteArray>

struct MidiReprocessResult {
    opendspx::MidiIntermediateData mediate;
    QList<MidiImportTrackInfo> trackInfos;
    QString errorMessage;
};

// Background re-derivation of the MIDI track layout after the user toggles
// "separate MIDI channels" in the import dialog. Runs off the UI thread;
// stale generations are dropped by the session via the generation id.
class MidiReprocessTask final : public Task {
public:
    MidiReprocessTask(QByteArray rawData, bool separateChannels);

    MidiReprocessResult takeResult();

private:
    void runTask() override;

    QByteArray m_rawData;
    bool m_separateChannels = false;
    MidiReprocessResult m_result;
};

#endif // DS_EDITOR_LITE_MIDIREPROCESSTASK_H
