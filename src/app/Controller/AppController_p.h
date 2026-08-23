#ifndef APPCONTROLLER_P_H
#define APPCONTROLLER_P_H

#include <QObject>

class AppController;
class IMainWindow;

class AppControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(AppController)

public:
    explicit AppControllerPrivate(AppController *q) : q_ptr(q) {
    }

    ~AppControllerPrivate() override = default;

    IMainWindow *m_mainWindow = nullptr;
    static void initializeModules();

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
