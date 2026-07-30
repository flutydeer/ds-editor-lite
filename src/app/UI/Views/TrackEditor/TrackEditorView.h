//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include "Interface/EditorViewState.h"
#include <lite/History/HistoryFocus.h>
#include "UI/Views/Common/PanelView.h"


class ITrackEditorCanvas;
class TrackListView;
class TimelineView;
class TempoLaneView;
class TimeSignatureLaneView;
class InfoLaneHeaderView;
class TrackViewModel;
class QSplitter;
class QVBoxLayout;
class ChannelView;

class TrackEditorView final : public PanelView {
    Q_OBJECT

public:
    explicit TrackEditorView(QWidget *parent = nullptr);
    ~TrackEditorView() override;

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

    TrackListView *m_trackListView;
    TimelineView *m_timeline;
    TempoLaneView *m_tempoLane;
    InfoLaneHeaderView *m_tempoLaneHeader;
    TimeSignatureLaneView *m_timeSignatureLane;
    InfoLaneHeaderView *m_timeSignatureLaneHeader;
    QSplitter *m_splitter;

    class ViewModel {
    public:
        QList<TrackViewModel *> tracks;
        TrackViewModel *findTrack(const Track *dsTrack);
    };

    ViewModel m_viewModel;
    void onTrackInserted(Track *dsTrack, qsizetype trackIndex);
    void onTrackPropertyChanged() const;
    void onTrackRemoved(const Track *dsTrack, qsizetype index);
    void setSelectedTrackIndex(int trackIndex) const;
    void syncSelectedTrackToList(int trackIndex) const;
    void connectCanvas();
    void scheduleLegacyFallback(const QString &reason);
    void replaceCanvasWithLegacy();

    ITrackEditorCanvas *m_canvas = nullptr;
    QVBoxLayout *m_trackTimelineAndViewLayout = nullptr;
    bool m_fallbackPending = false;
};



#endif // TRACKSVIEW_H
