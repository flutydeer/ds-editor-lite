#ifndef TRACKEDITORCONTEXTMENUCONTROLLER_H
#define TRACKEDITORCONTEXTMENUCONTROLLER_H

#include <lite/ProjectModel/AppModel/AudioInfoModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>

#include <QObject>
#include <QPoint>
#include <QVector>

class QWidget;

struct TrackEditorMenuContext {
    enum class Target { Background, SingingClip, AudioClip };

    Target target = Target::Background;
    QPoint globalPos;
    int rawTick = 0;
    int snappedTick = 0;
    int trackIndex = -1;
    int clipId = -1;
    QList<int> selectedClipIds;
    bool audioMissing = false;
};

struct TrackPastePreviewNote {
    int start = 0;
    int length = 0;
    int key = 0;
};

struct TrackPastePreviewClip {
    IClip::ClipType type = IClip::Generic;
    Clip::ClipCommonProperties properties;
    int trackIndexOffset = 0;
    QString defaultLanguage;
    QVector<TrackPastePreviewNote> notes;
    QString audioPath;
    AudioInfoModel audioInfo;
};

struct TrackPastePreviewData {
    QVector<TrackPastePreviewClip> clips;
};

class ITrackPastePreviewHost {
public:
    virtual ~ITrackPastePreviewHost() = default;
    virtual void showTrackPastePreview(const TrackPastePreviewData &data, int previewTick,
                                       int baseTrackIndex) = 0;
    virtual void clearTrackPastePreview() = 0;
};

class TrackEditorContextMenuController final : public QObject {
public:
    explicit TrackEditorContextMenuController(QWidget *owner);

    void showMenu(const TrackEditorMenuContext &context, ITrackPastePreviewHost *previewHost);
    void cutSelection() const;
    void copySelection() const;
    void pasteSelection() const;
    void deleteSelection(const QList<int> &clipIds = {}) const;
    void selectAll() const;

private:
    void insertAudioClip(int trackIndex, int tick) const;
    void relocateAudioClip(int clipId) const;

    QWidget *m_owner = nullptr;
};

#endif // TRACKEDITORCONTEXTMENUCONTROLLER_H
