//
// Created by FlutyDeer on 2025/6/3.
//

#ifndef MIXCONSOLEVIEW_H
#define MIXCONSOLEVIEW_H

#include "Model/AppModel/AppModel.h"
#include "UI/Views/Common/ITabPanelPage.h"

#include <QWidget>

class Track;
class QListWidget;
class ChannelView;

class MixConsoleView : public QWidget, public ITabPanelPage {
    Q_OBJECT

public:
    [[nodiscard]] QString tabId() const override;
    [[nodiscard]] QString tabName() const override;
    [[nodiscard]] AppGlobal::PanelType panelType() const override;
    [[nodiscard]] QWidget *toolBar() override;
    [[nodiscard]] QWidget *content() override;

    explicit MixConsoleView(QWidget *parent = nullptr);

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onMasterControlChanged(const TrackControl &control);
    void onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const;

private slots:
    void onTrackInserted(Track *dsTrack, qsizetype trackIndex);
    void onTrackRemoved(qsizetype index);

private:
    void onTrackPropertyChanged() const;

    QListWidget *m_channelListView;

    ChannelView *m_masterChannel;

    QWidget *m_placeHolder;
};


#endif //MIXCONSOLEVIEW_H