//
// Created by fluty on 24-7-21.
//

#ifndef APPCONTROLLER_P_H
#define APPCONTROLLER_P_H

#include "Global/AppGlobal.h"
#include <QObject>

class AppController;
class IMainWindow;
class IPanel;

class AppControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(AppController)

public:
    explicit AppControllerPrivate(AppController *q) : q_ptr(q) {
    }

    ~AppControllerPrivate() override = default;

    IMainWindow *m_mainWindow = nullptr;
    QList<IPanel *> m_panels{};
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;

    static void initializeModules();
    static bool isPowerOf2(int num);

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
