#ifndef TRACKLISTHEADERVIEW_H
#define TRACKLISTHEADERVIEW_H

#include <QWidget>

class QAbstractButton;
class QAction;
class QMenu;

class TrackListHeaderView final : public QWidget {
    Q_OBJECT

public:
    explicit TrackListHeaderView(QWidget *parent = nullptr);

    [[nodiscard]] bool tempoLaneVisible() const;
    [[nodiscard]] bool timeSignatureLaneVisible() const;

signals:
    void tempoLaneToggled(bool visible);
    void timeSignatureLaneToggled(bool visible);

private:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    QAbstractButton *m_btnNewTrack = nullptr;
    QAbstractButton *m_btnLaneOptions = nullptr;
    QMenu *m_laneMenu = nullptr;
    QAction *m_actionTempoLane = nullptr;
    QAction *m_actionTimeSignatureLane = nullptr;
};


#endif // TRACKLISTHEADERVIEW_H
