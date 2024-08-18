//
// Created by fluty on 2024/2/5.
//

#ifndef TRACKLISTHEADERVIEW_H
#define TRACKLISTHEADERVIEW_H

#include <QWidget>

class TrackListHeaderView final : public QWidget {
    Q_OBJECT

public:
    explicit TrackListHeaderView(QWidget *parent = nullptr);

private:
    void paintEvent(QPaintEvent *event) override;
};



#endif // TRACKLISTHEADERVIEW_H
