#include "MidiPortList.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

MidiPortList::MidiPortList(const QStringList &names) : m_names(names) {
    QHash<QString, int> occurrences;
    m_ids.reserve(m_names.size());
    for (const auto &name : m_names) {
        const auto occurrence = occurrences[name]++;
        // JSON keeps names that contain separators unambiguous.
        m_ids.append(QString::fromUtf8(
            QJsonDocument(QJsonArray{name, occurrence}).toJson(QJsonDocument::Compact)));
    }
}

const QStringList &MidiPortList::names() const {
    return m_names;
}

const QStringList &MidiPortList::ids() const {
    return m_ids;
}

bool MidiPortList::isEmpty() const {
    return m_ids.isEmpty();
}

bool MidiPortList::contains(const QString &id) const {
    return m_ids.contains(id);
}

MidiPortList::DeviceAction MidiPortList::deviceAction(const QString &desiredId,
                                                      const bool deviceOpen,
                                                      const MidiPortList &previous) const {
    if (deviceOpen) {
        if (!contains(desiredId))
            return DeviceAction::Close;
        // Nothing in the enumeration tells the survivor of a vanished duplicate
        // apart from the port that disappeared.
        return countOfName(desiredId) < previous.countOfName(desiredId) ? DeviceAction::Close
                                                                        : DeviceAction::None;
    }
    return !desiredId.isEmpty() && contains(desiredId) ? DeviceAction::Open : DeviceAction::None;
}

int MidiPortList::countOfName(const QString &id) const {
    const auto name = nameOf(id);
    return name.isEmpty() ? 0 : int(m_names.count(name));
}

QString MidiPortList::nameOf(const QString &id) {
    const auto identity = QJsonDocument::fromJson(id.toUtf8()).array();
    return identity.isEmpty() ? QString() : identity.first().toString();
}
