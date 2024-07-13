//
// Created by fluty on 2024/7/13.
//

#ifndef IMAINWINDOW_H
#define IMAINWINDOW_H

#include <QString>

class IMainWindow {
public:
    virtual ~IMainWindow() = default;
    virtual void setProjectName(const QString &name) = 0;
};



#endif // IMAINWINDOW_H
