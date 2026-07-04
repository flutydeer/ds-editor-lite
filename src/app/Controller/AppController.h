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
#include <mutex>

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
    void initializeLanguageEngine();

    [[nodiscard]] QString lastProjectFolder() const;
    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QStringList recentProjectFiles() const;
    void setProjectName(const QString &name);
    void clearRecentProjectFiles();
    void removeRecentProjectFile(const QString &filePath);
    void registerPanel(IPanel *panel);

public slots:
    void quit();
    void restart();
    void newProject();
    bool openFile(const QString &filePath, QString &errorMessage);
    void requestOpenFile(const QString &filePath);
    bool saveProject(const QString &filePath, QString &errorMessage);

    void setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed);

    static void importMidiFile(const QString &filePath);
    static void exportMidiFile(const QString &filePath);


    static void onSetTempo(double tempo);
    void onSetTimeSignature(int numerator, int denominator);
    static void editMasterControl(const TrackControl &control);
    void setActivePanel(AppGlobal::PanelType panel);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                           const QString &redoActionName);

signals:
    void activePanelChanged(AppGlobal::PanelType panel);
    void recentProjectFilesChanged(const QStringList &filePaths);

private:
    Q_DECLARE_PRIVATE(AppController)
    // QScopedPointer<AppControllerPrivate> d_ptr;
    AppControllerPrivate *d_ptr;
    std::once_flag m_languageEngineInitialized{};
};

#endif // APPCONTROLLER_H
