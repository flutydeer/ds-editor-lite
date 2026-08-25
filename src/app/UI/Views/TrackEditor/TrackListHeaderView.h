#ifndef TRACKLISTHEADERVIEW_H
#define TRACKLISTHEADERVIEW_H

#include <QColor>
#include <QWidget>

class QAbstractButton;
class DividerLine;

class TrackListHeaderView final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor iconCheckedColor READ iconCheckedColor WRITE setIconCheckedColor)

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
    void resizeEvent(QResizeEvent *event) override;
    void updateBottomDividerVisibility();

    // Theme color accessor (QSS-overridable via qproperty-*); the setter
    // re-tints the checked state of the lane toggle icon
    [[nodiscard]] QColor iconCheckedColor() const;
    void setIconCheckedColor(const QColor &color);
    void rebuildToggleIcons();

    QColor m_iconCheckedColor = {155, 186, 255};
    QAbstractButton *m_btnNewTrack = nullptr;
    QAbstractButton *m_btnToggleTempoLane = nullptr;
    QAbstractButton *m_btnToggleTimeSignatureLane = nullptr;
    DividerLine *m_bottomDivider = nullptr;
};



#endif // TRACKLISTHEADERVIEW_H
