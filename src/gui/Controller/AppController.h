//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSCONTROLLER_H
#define TRACKSCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Views/TracksView.h"

class AppController final : public QObject, public Singleton<AppController>{
    Q_OBJECT

public:
    explicit AppController() = default;
    ~AppController() override = default;

public slots:
    void onNewProject();
    void openProject(const QString &filePath);
    void importMidiFile(const QString &filePath);
    void onRunG2p();
    void onSetTempo(double tempo);
    void onSetTimeSignature(int numerator, int denominator);

private:
    bool isPowerOf2(int num);
};

// using ControllerSingleton = Singleton<Controller>;

#endif // TRACKSCONTROLLER_H
