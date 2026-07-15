//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#define appController AppController::instance()

#include "Global/AppGlobal.h"
#include "Utils/Singleton.h"

#include <QObject>
#include <QStringList>

class AppControllerPrivate;
class IMainWindow;
class IPanel;
class AppModel;
class DecodeAudioTask;
class AudioClip;
class TrackControl;

class AppController final : public QObject {
    Q_OBJECT

private:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(AppController)
    Q_DISABLE_COPY_MOVE(AppController)

public:
    void setMainWindow(IMainWindow *window);

    void registerPanel(IPanel *panel);

public slots:
    void quit();
    void restart();
    void setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed);

    bool exportMidiFile(const QString &filePath);


    static void onSetTempo(double tempo);
    void onSetTimeSignature(int numerator, int denominator);
    static void editMasterControl(const TrackControl &control);
    void setActivePanel(AppGlobal::PanelType panel);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                           const QString &redoActionName);

signals:
    void activePanelChanged(AppGlobal::PanelType panel);

private:
    Q_DECLARE_PRIVATE(AppController)
    // QScopedPointer<AppControllerPrivate> d_ptr;
    AppControllerPrivate *d_ptr;
};

#endif // APPCONTROLLER_H
