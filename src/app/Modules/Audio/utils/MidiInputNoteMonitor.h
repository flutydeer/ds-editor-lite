#ifndef AUDIO_MIDIINPUTNOTEMONITOR_H
#define AUDIO_MIDIINPUTNOTEMONITOR_H

#include <QObject>

#include <TalcsMidi/MidiMessageListener.h>

namespace talcs {
    class MidiInputDevice;
    class MidiMessage;
}

/// Mirrors the notes an external MIDI keyboard is holding. The device reports on
/// its own thread, so the signals are queued to the object's thread (the GUI
/// thread wherever a view displays them).
class MidiInputNoteMonitor : public QObject, public talcs::MidiMessageListener {
    Q_OBJECT
public:
    explicit MidiInputNoteMonitor(QObject *parent = nullptr);
    ~MidiInputNoteMonitor() override;

    /// Starts watching \p source (the MIDI system's integrator) and stops
    /// watching the previous one. The source outlives every settings page, so
    /// the pointer is not tracked beyond detaching in the destructor.
    void attachTo(talcs::MidiMessageListener *source);

signals:
    void notePressed(int key);
    void noteReleased(int key);
    /// Every note let go at once: an all-notes-off control message or the
    /// device stopping.
    void allNotesReleased();

protected:
    bool processMessage(const talcs::MidiMessage &message) override;
    bool processDeviceWillStart(talcs::MidiInputDevice *device) override;
    void processDeviceStopped() override;

private:
    talcs::MidiMessageListener *m_source = nullptr;
};

#endif // AUDIO_MIDIINPUTNOTEMONITOR_H
