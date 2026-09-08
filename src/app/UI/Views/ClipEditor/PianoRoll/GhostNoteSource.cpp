#include "GhostNoteSource.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Views/Common/EditorItemGeometry.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
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
    const auto width = std::max<double>(EditorItemGeometry::noteMinimumVisualWidth,
                                        right - left - inset * 2.0);
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
    m_enabled = appOptions->appearance()->showGhostNotes && !m_hostClip.isNull();
    m_notes.clear();
    m_maxLength = 0;
    if (!m_enabled) {
        clearConnections();
        emit changed();
        return;
    }
    rebuildConnections();

    // If the host track cannot be resolved, fall back to excluding just the host clip so
    // that no track disappears and the host's own notes are not duplicated
    Track *hostTrack = nullptr;
    appModel->findClipById(m_hostClip->id(), hostTrack);

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
                m_notes.append({note->globalStart(), note->length(), note->keyIndex(), colorIndex});
                m_maxLength = std::max(m_maxLength, note->length());
            }
        }
    }
    std::sort(m_notes.begin(), m_notes.end(), [](const GhostNote &a, const GhostNote &b) {
        return a.globalStart < b.globalStart;
    });
    emit changed();
}

void GhostNoteSource::rebuildConnections() {
    for (const auto *track : appModel->tracks()) {
        disconnect(track, nullptr, this, nullptr);
        connect(track, &Track::propertyChanged, this, &GhostNoteSource::scheduleRebuild);
        connect(track, &Track::clipChanged, this,
                [this](Track::ClipChangeType, Clip *) { scheduleRebuild(); });
        for (const auto *clip : track->clips()) {
            disconnect(clip, nullptr, this, nullptr);
            connect(clip, &Clip::propertyChanged, this, &GhostNoteSource::scheduleRebuild);
            if (const auto *singingClip = qobject_cast<const SingingClip *>(clip))
                connect(singingClip, &SingingClip::noteChanged, this,
                        &GhostNoteSource::scheduleRebuild);
        }
    }
    m_connectionsBuilt = true;
}

void GhostNoteSource::clearConnections() {
    if (!m_connectionsBuilt)
        return;
    // Once the option is off, stop watching the model so nothing is traversed for nothing
    for (const auto *track : appModel->tracks()) {
        disconnect(track, nullptr, this, nullptr);
        for (const auto *clip : track->clips())
            disconnect(clip, nullptr, this, nullptr);
    }
    m_connectionsBuilt = false;
}
