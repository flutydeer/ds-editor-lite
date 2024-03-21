//
// Created by fluty on 24-3-19.
//

#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>

class Window : public QWidget {
    Q_OBJECT

public:
    explicit Window(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
};



#endif // WINDOW_H
