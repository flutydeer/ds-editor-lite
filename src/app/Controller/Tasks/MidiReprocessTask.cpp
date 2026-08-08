#include "MidiReprocessTask.h"

#include <opendspxconverter/midi/midiconverter.h>

#include <QCoreApplication>

#include <sstream>
#include <utility>

MidiReprocessTask::MidiReprocessTask(QByteArray rawData, const bool separateChannels)
    : m_rawData(std::move(rawData)), m_separateChannels(separateChannels) {
}

MidiReprocessResult MidiReprocessTask::takeResult() {
    return std::move(m_result);
}

void MidiReprocessTask::runTask() {
    if (isTerminateRequested())
        return;
    opendspx::MidiConverter converter;
    opendspx::MidiConverter::Error error;
    std::stringstream ss(m_rawData.toStdString(), std::ios::in);
    auto updated = converter.convertMidiToIntermediate(ss, error, {m_separateChannels});
    if (isTerminateRequested())
        return;
    if (error != opendspx::MidiConverter::Error::NoError) {
        m_result.errorMessage =
            QCoreApplication::translate("MidiReprocessTask",
                                        "Failed to re-parse MIDI channels.\ntype: %L1")
                .arg(static_cast<int>(error));
        return;
    }
    m_result.mediate = std::move(updated);
    m_result.trackInfos = buildMidiTrackInfoList(m_result.mediate.tracks());
}
