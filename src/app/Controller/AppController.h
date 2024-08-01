//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSCONTROLLER_H
#define TRACKSCONTROLLER_H

#define appController AppController::instance()

#include "Global/AppGlobal.h"
#include "Utils/Singleton.h"

#include <QObject>

class AppControllerPrivate;
class IMainWindow;
class IPanel;
class AppModel;
class DecodeAudioTask;
class AudioClip;

class AppController final : public QObject, public Singleton<AppController> {
    Q_OBJECT

public:
    explicit AppController();
    ~AppController() override;
    void setMainWindow(IMainWindow *window);

    [[nodiscard]] QString lastProjectFolder()const;
    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] QString projectName() const;
    void setProjectName(const QString &name);

    [[nodiscard]] bool isLanguageEngineReady() const;
    void registerPanel(IPanel *panel);

public slots:
    void newProject();
    void openProject(const QString &filePath);
    bool saveProject(const QString &filePath);

    void importMidiFile(const QString &filePath);
    void exportMidiFile(const QString &filePath);

    void importAproject(const QString &filePath);

    void onSetTempo(double tempo);
    void onSetTimeSignature(int numerator, int denominator);
    void onSetQuantize(int quantize);
    void selectTrack(int trackIndex);
    void onPanelClicked(AppGlobal::PanelType panel);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                          const QString &redoActionName);

signals:
    void activePanelChanged(AppGlobal::PanelType panel);

private:
    Q_DECLARE_PRIVATE(AppController)
    // QScopedPointer<AppControllerPrivate> d_ptr;
    AppControllerPrivate * d_ptr;
};

#endif // TRACKSCONTROLLER_H
