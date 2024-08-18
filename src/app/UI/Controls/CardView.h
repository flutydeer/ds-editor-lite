//
// Created by fluty on 24-3-18.
//

#ifndef CARDVIEW_H
#define CARDVIEW_H

#include <QWidget>

class CardView : public QWidget {
    Q_OBJECT

public:
    explicit CardView(QWidget *parent = nullptr);
};

inline CardView::CardView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setContentsMargins({});
}

#endif // CARDVIEW_H
