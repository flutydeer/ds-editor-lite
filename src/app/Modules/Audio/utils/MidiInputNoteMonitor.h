#ifndef AUDIO_MIDIINPUTNOTEMONITOR_H
#define AUDIO_MIDIINPUTNOTEMONITOR_H

#include <QObject>
#include <QAtomicInteger>

#include <TalcsMidi/MidiMessageListener.h>

namespace talcs {
    class MidiInputDevice;
    class MidiMessage;
}

/// Mirrors the notes an external MIDI keyboard is holding. Device callbacks may
/// run on another thread, so note updates are delivered on this object's thread
/// and can be reset there.
class MidiInputNoteMonitor : public QObject, public talcs::MidiMessageListener {
    Q_OBJECT
public:
    explicit MidiInputNoteMonitor(QObject *parent = nullptr);
    ~MidiInputNoteMonitor() override;

    /// Starts watching \p source (the MIDI system's integrator) and stops
    /// watching the previous one. The source outlives every settings page, so
    /// the pointer is not tracked beyond detaching in the destructor.
    void attachTo(talcs::MidiMessageListener *source);

    /// Releases displayed keys and discards earlier queued note notifications.
    /// Call on the monitor's thread.
    void resetHeldNotes();

signals:
    void notePressed(int key);
    void noteReleased(int key);
    /// Every note let go at once: a reset, an all-notes-off control message,
    /// or the device stopping.
    void allNotesReleased();

protected:
    bool processMessage(const talcs::MidiMessage &message) override;
    bool processDeviceWillStart(talcs::MidiInputDevice *device) override;
    void processDeviceStopped() override;

private:
    talcs::MidiMessageListener *m_source = nullptr;
    QAtomicInteger<quint64> m_notificationGeneration = 0;
};

#endif // AUDIO_MIDIINPUTNOTEMONITOR_H
