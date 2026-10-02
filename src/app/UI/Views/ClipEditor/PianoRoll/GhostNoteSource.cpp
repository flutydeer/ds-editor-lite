#include "GhostNoteSource.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Views/Common/EditorItemGeometry.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QTimer>

#include <algorithm>

QColor GhostNoteStyle::fillColor(const int colorIndex) {
    auto color = AppColorPalette::instance()->noteBackground(colorIndex);
    color.setAlphaF(color.alphaF() * opacity);
    return color;
}

QRectF GhostNoteStyle::barRect(const double left, const double right, const double top,
                               const double height) {
    constexpr auto inset = EditorItemGeometry::noteBorderWidth * 0.5;
    // Too narrow to afford the gap: fall back to noteMinimumVisualWidth rather than
    // painting nothing at all
    const auto width =
        std::max<double>(EditorItemGeometry::noteMinimumVisualWidth, right - left - inset * 2.0);
    return {(left + right - width) * 0.5, top, width, height};
}

GhostNoteSource::GhostNoteSource(QObject *parent) : QObject(parent) {
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::Appearance || option == AppOptionsGlobal::All)
                    scheduleRebuild();
            });
    connect(appModel, &AppModel::modelChanged, this, &GhostNoteSource::scheduleRebuild);
    connect(appModel, &AppModel::trackChanged, this, &GhostNoteSource::scheduleRebuild);
}

void GhostNoteSource::setHostClip(SingingClip *clip) {
    if (m_hostClip == clip)
        return;
    m_hostClip = clip;
    scheduleRebuild();
}

bool GhostNoteSource::enabled() const {
    return m_enabled;
}

const QList<GhostNote> &GhostNoteSource::notes() const {
    return m_notes;
}

int GhostNoteSource::maxLength() const {
    return m_maxLength;
}

void GhostNoteSource::scheduleRebuild() {
    if (m_rebuildScheduled)
        return;
    m_rebuildScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_rebuildScheduled = false;
        rebuild();
    });
}

void GhostNoteSource::rebuild() {
    const bool enabled = appOptions->appearance()->showGhostNotes && !m_hostClip.isNull();
    QList<GhostNote> notes;
    int maxLength = 0;
    if (enabled) {
        // If the host track cannot be resolved, fall back to excluding just the host clip
        // so that no track disappears and the host's own notes are not duplicated
        Track *hostTrack = nullptr;
        appModel->findClipById(m_hostClip->id(), hostTrack);
        rebuildConnections(hostTrack);

        for (const auto *track : appModel->tracks()) {
            if (track == hostTrack)
                continue;
            const auto colorIndex = track->colorIndex();
            for (const auto *clip : track->clips()) {
                if (clip == m_hostClip || clip->clipType() != Clip::Singing)
                    continue;
                const auto *singingClip = qobject_cast<const SingingClip *>(clip);
                if (!singingClip)
                    continue;
                for (const auto *note : singingClip->notes()) {
                    notes.append(
                        {note->globalStart(), note->length(), note->keyIndex(), colorIndex});
                    maxLength = std::max(maxLength, note->length());
                }
            }
        }
        std::sort(notes.begin(), notes.end(), [](const GhostNote &a, const GhostNote &b) {
            return a.globalStart < b.globalStart;
        });
    } else {
        clearConnections();
    }

    // Model signals can fire for edits that leave the ghost list untouched (word or
    // pronunciation changes, host clip edits, ...); keep the old list and stay quiet
    // unless something the renderers read has actually changed.
    if (enabled == m_enabled && maxLength == m_maxLength && notes == m_notes)
        return;
    m_enabled = enabled;
    m_maxLength = maxLength;
    m_notes = std::move(notes);
    emit changed();
}

void GhostNoteSource::rebuildConnections(const Track *hostTrack) {
    for (const auto *track : appModel->tracks()) {
        if (track == hostTrack)
            continue; // the host track never contributes ghosts, so it is never watched
        if (!m_connectedTracks.contains(track)) {
            connect(track, &Track::propertyChanged, this, &GhostNoteSource::scheduleRebuild);
            connect(track, &Track::clipChanged, this,
                    [this](Track::ClipChangeType, Clip *) { scheduleRebuild(); });
            connect(track, &Track::destroyed, this,
                    [this, track] { m_connectedTracks.remove(track); });
            m_connectedTracks.insert(track);
        }
        for (const auto *clip : track->clips()) {
            if (clip == m_hostClip || clip->clipType() != Clip::Singing)
                continue;
            if (m_connectedClips.contains(clip))
                continue;
            connect(clip, &Clip::propertyChanged, this, &GhostNoteSource::scheduleRebuild);
            connect(static_cast<const SingingClip *>(clip), &SingingClip::noteChanged, this,
                    [this](SingingClip::NoteChangeType type, const QList<Note *> &) {
                        // Only geometry-affecting changes can alter the ghost list; word,
                        // pronunciation and phoneme edits leave the bars untouched
                        if (type == SingingClip::Insert || type == SingingClip::Remove ||
                            type == SingingClip::TimeKeyPropertyChange)
                            scheduleRebuild();
                    });
            connect(clip, &Clip::destroyed, this, [this, clip] { m_connectedClips.remove(clip); });
            m_connectedClips.insert(clip);
        }
    }
    m_connectionsBuilt = true;
}

void GhostNoteSource::clearConnections() {
    if (!m_connectionsBuilt)
        return;
    // Once the option is off, stop watching the model so nothing is traversed for nothing
    for (const auto *track : std::as_const(m_connectedTracks))
        disconnect(track, nullptr, this, nullptr);
    for (const auto *clip : std::as_const(m_connectedClips))
        disconnect(clip, nullptr, this, nullptr);
    m_connectedTracks.clear();
    m_connectedClips.clear();
    m_connectionsBuilt = false;
}
