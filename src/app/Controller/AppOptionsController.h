//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONSCONTROLLER_H
#define APPOPTIONSCONTROLLER_H

#include <QObject>
#include "Utils/Singleton.h"
#include "AppOptions/AppearanceOptionController.h"

class AppOptionsController final : public QObject, public Singleton<AppOptionsController> {
    Q_OBJECT

public:
    explicit AppOptionsController();

    AppearanceOptionController *appearanceController();

public slots:
    bool apply();
    void cancel();

signals:
    void canApplyChanged(bool on);

private slots:
    void onSubOptionModified();

private:
    AppearanceOptionController m_appearanceController;

    bool m_modified = false;
};

#endif // APPOPTIONSCONTROLLER_H
