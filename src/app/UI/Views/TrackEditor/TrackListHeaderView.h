//
// Created by fluty on 2024/2/5.
//

#ifndef TRACKLISTHEADERVIEW_H
#define TRACKLISTHEADERVIEW_H

#include <QWidget>

class QAbstractButton;

class TrackListHeaderView final : public QWidget {
    Q_OBJECT

public:
    explicit TrackListHeaderView(QWidget *parent = nullptr);

private:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    QAbstractButton *m_btnNewTrack = nullptr;
};



#endif // TRACKLISTHEADERVIEW_H
