//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSCONTROLLER_H
#define TRACKSCONTROLLER_H

#include <QObject>
#include <QStandardPaths>

#include "Utils/Singleton.h"
#include "Global/AppGlobal.h"

class IMainWindow;
class IPanel;
class LaunchLanguageEngineTask;
class AppModel;
class DecodeAudioTask;
class AudioClip;

class AppController final : public QObject, public Singleton<AppController> {
    Q_OBJECT

public:
    explicit AppController();
    ~AppController() override = default;
    void setMainWindow(IMainWindow *window);

    [[nodiscard]] QString lastProjectFolder()const;
    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] QString projectName() const;
    void setProjectName(const QString &name);

    [[nodiscard]] bool isLanguageEngineReady() const;
    void registerPanel(IPanel *panel);

public slots:
    void onNewProject();
    void openProject(const QString &filePath);
    void saveProject(const QString &filePath);

    void importMidiFile(const QString &filePath);
    void exportMidiFile(const QString &filePath);

    void importAproject(const QString &filePath);

    void onSetTempo(double tempo);
    void onSetTimeSignature(int numerator, int denominator);
    void onSetQuantize(int quantize);
    void onTrackSelectionChanged(int trackIndex);
    void onPanelClicked(AppGlobal::PanelType panel);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                          const QString &redoActionName);

signals:
    void activatedPanelChanged(AppGlobal::PanelType panel);

private:
    const QString defaultProjectName = tr("New Project");

    bool isPowerOf2(int num);
    void handleRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task);
    void updateProjectPathAndName(const QString &path);

    IMainWindow *m_mainWindow = nullptr;
    QString m_lastProjectFolder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString m_projectPath;
    QString m_projectName;
    bool m_isLanguageEngineReady = false;
    QList<IPanel *> m_panels;
    AppGlobal::PanelType m_activatedPanel = AppGlobal::TracksEditor;
};

// using ControllerSingleton = Singleton<Controller>;

#endif // TRACKSCONTROLLER_H
