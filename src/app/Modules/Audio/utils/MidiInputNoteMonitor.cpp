#include "MidiInputNoteMonitor.h"

#include <TalcsMidi/MidiMessage.h>

MidiInputNoteMonitor::MidiInputNoteMonitor(QObject *parent) : QObject(parent) {
}

MidiInputNoteMonitor::~MidiInputNoteMonitor() {
    if (m_source)
        m_source->removeFilter(this);
}

void MidiInputNoteMonitor::attachTo(talcs::MidiMessageListener *source) {
    if (m_source == source)
        return;
    if (m_source)
        m_source->removeFilter(this);
    m_source = source;
    if (m_source)
        m_source->addFilter(this);
}

bool MidiInputNoteMonitor::processMessage(const talcs::MidiMessage &message) {
    // Note on first: a note on with velocity zero is the running status form of
    // a note off, which isNoteOff() also reports.
    if (message.isNoteOn())
        emit notePressed(message.getNoteNumber());
    else if (message.isNoteOff())
        emit noteReleased(message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        emit allNotesReleased();
    return false;
}

bool MidiInputNoteMonitor::processDeviceWillStart(talcs::MidiInputDevice *device) {
    Q_UNUSED(device)
    emit allNotesReleased();
    return true;
}

void MidiInputNoteMonitor::processDeviceStopped() {
    emit allNotesReleased();
}
