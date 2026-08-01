#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include "Interface/EditorViewState.h"
#include <lite/History/HistoryFocus.h>
#include "UI/Views/Common/PanelView.h"


class AudioClip;
class SingingClip;
class TrackListView;
class TracksGraphicsView;
class TracksGraphicsScene;
class TracksRhiWidget;
class TimelineView;
class TempoLaneView;
class TimeSignatureLaneView;
class InfoLaneHeaderView;
class TrackEditorBackgroundView;
class TrackViewModel;
class TrackEditorContextMenuController;
class AbstractClipView;
class QSplitter;
class ChannelView;
class QVBoxLayout;

class TrackEditorView final : public PanelView {
    Q_OBJECT

public:
    explicit TrackEditorView(QWidget *parent = nullptr);
    ~TrackEditorView() override;

    AbstractClipView *findClipItemById(int id) const;
    [[nodiscard]] TrackPanelViewState viewState() const;
    bool centerAt(double tick, double trackIndex) const;
    bool setViewScale(double horizontalScale, double verticalScale) const;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus, bool animated) const;

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onTrackMoved(qsizetype from, qsizetype to);
    void onClipChanged(Track::ClipChangeType type, Clip *clip, const Track *dsTrack);
    void onPositionChanged(double tick) const;
    void onLastPositionChanged(double tick) const;

signals:
    void trackCountChanged(qsizetype count);
    void setPositionTriggered(double tick);

private slots:
    void onViewScaleChanged(qreal sx, qreal sy) const;
    static void onRemoveTrackTriggered(int id);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void createLegacyBackend();
    void connectLegacyBackend();
    void connectRhiBackend();
    void fallbackToLegacy();
    void registerEditorShortcuts();
    void populateLegacyClipItems();
    [[nodiscard]] double activeScaleY() const;

    TrackListView *m_trackListView = nullptr;
    TracksGraphicsView *m_graphicsView = nullptr;
    TracksGraphicsScene *m_tracksScene = nullptr;
    TracksRhiWidget *m_rhiView = nullptr;
    TimelineView *m_timeline;
    TempoLaneView *m_tempoLane;
    InfoLaneHeaderView *m_tempoLaneHeader;
    TimeSignatureLaneView *m_timeSignatureLane;
    InfoLaneHeaderView *m_timeSignatureLaneHeader;
    TrackEditorBackgroundView *m_gridItem = nullptr;
    TrackEditorContextMenuController *m_contextMenuController = nullptr;
    QSplitter *m_splitter = nullptr;
    QVBoxLayout *m_trackTimelineAndViewLayout = nullptr;

    class ViewModel {
    public:
        QList<TrackViewModel *> tracks;
        TrackViewModel *findTrack(const Track *dsTrack);
    };

    ViewModel m_viewModel;
    QMap<int, AbstractClipView *> m_pendingRemoveClipViews;

    void onTrackInserted(Track *dsTrack, qsizetype trackIndex);
    void onClipInserted(Clip *clip, TrackViewModel *track, int trackIndex);
    void insertSingingClip(SingingClip *clip, TrackViewModel *track, int trackIndex);
    void insertAudioClip(AudioClip *clip, TrackViewModel *track, int trackIndex);
    void onClipRemoved(Clip *clip, TrackViewModel *track);
    void onTrackPropertyChanged() const;
    void updateClipOnView(Clip *clip);
    void onTrackRemoved(const Track *dsTrack, qsizetype index);
    void setSelectedTrackIndex(int trackIndex) const;
    void syncSelectedTrackToList(int trackIndex) const;
};



#endif // TRACKSVIEW_H
