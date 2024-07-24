//
// Created by fluty on 2024/7/11.
//

#ifndef TRACKLISTWIDGET_H
#define TRACKLISTWIDGET_H

#include <QListWidget>

class TrackListWidget : public QListWidget{
public:
    explicit TrackListWidget(QWidget *parent = nullptr);

private:
    void mousePressEvent(QMouseEvent *event) override;
};



#endif //TRACKLISTWIDGET_H
