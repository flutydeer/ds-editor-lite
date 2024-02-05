//
// Created by fluty on 2024/2/5.
//

#ifndef ACTIONBUTTONSVIEW_H
#define ACTIONBUTTONSVIEW_H

#include <QWidget>

class ActionButtonsView : public QWidget {
public:
    explicit ActionButtonsView(QWidget *parent = nullptr);

private:
    int m_contentHeight = 32;
};



#endif //ACTIONBUTTONSVIEW_H
