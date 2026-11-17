#ifndef AUDIO_MIDIPORTLIST_H
#define AUDIO_MIDIPORTLIST_H

#include <QString>
#include <QStringList>

/// The MIDI input ports present at one enumeration, each with an identity that
/// survives re-enumeration.
///
/// RtMidi reports nothing when a Windows MIDI device is unplugged, so the port
/// list is polled. Ports are identified by name plus their occurrence among the
/// ports of that name, not by index: indices shift as soon as the list changes,
/// so an index cannot tell an unplugged port from a reordered one.
class MidiPortList {
public:
    MidiPortList() = default;
    explicit MidiPortList(const QStringList &names);

    const QStringList &names() const;
    const QStringList &ids() const;
    bool isEmpty() const;
    bool contains(const QString &id) const;

    bool operator==(const MidiPortList &) const = default;

    /// What this list implies for a device opened from an earlier one.
    enum class DeviceAction {
        None,  ///< The device state still matches the list.
        Close, ///< The port of the open device is no longer listed.
        Open,  ///< The desired port is listed but its device is not open.
    };

    /// \p deviceOpen tells whether the device for \p desiredId is open, and
    /// \p previous is the list seen before this one, if any.
    ///
    /// Occurrence numbers restart at zero on every enumeration, so when a
    /// duplicate-name port disappears its survivor takes over the id that the
    /// vanished port held, and the open device is closed to be safe.
    DeviceAction deviceAction(const QString &desiredId, bool deviceOpen,
                              const MidiPortList &previous = {}) const;

    /// The port name encoded in \p id, or an empty string when \p id holds none.
    static QString nameOf(const QString &id);

private:
    /// How many listed ports share the name of \p id.
    int countOfName(const QString &id) const;

    QStringList m_names;
    QStringList m_ids;
};

#endif // AUDIO_MIDIPORTLIST_H
