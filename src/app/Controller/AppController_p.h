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
class LaunchLanguageEngineTask;
class OpenDspxProjectTask;
class ProgressDialog;
class TaskStatus;

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
    void updateProjectPathAndName(const QString &path);
    void addRecentProjectFile(const QString &path);

    bool openDspxFile(const QString &path, QString &errorMessage);
    bool openMidiFile(const QString &path, QString &errorMessage);
    bool openFileAndActivateFirstClip(const QString &filePath);
    void requestOpenDspxFile(const QString &filePath);
    void waitAndOpenDspxFile();
    void startOpenDspxTask();
    void handleOpenDspxTaskFinished(OpenDspxProjectTask *task);
    void updateOpenProjectDialog(const TaskStatus &status) const;
    void showOpenProjectError(const QString &errorMessage) const;
    void activateFirstClip();
    void cancelPendingOpen();
    void handlePendingOpenPackageStatus(AppStatus::ModuleStatus status);
    void createPendingOpenDialog();
    void clearPendingOpenDialog(bool deleteImmediately = false);
    bool confirmOpenWithoutPackageMetadata();

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
