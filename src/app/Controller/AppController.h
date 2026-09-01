#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#define appController AppController::instance()

#include <lite/Core/Singleton.h>

#include <QObject>
#include <QStringList>

class AppControllerPrivate;
class IMainWindow;
class AppModel;
class DecodeAudioTask;
class AudioClip;
class TrackControl;
class AppContext;

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

public slots:
    void quit();
    void restart();

    bool exportMidiFile(const QString &filePath);

    static void onSetTempo(double tempo);
    // Inserts a tempo point at tick, or edits the existing point there.
    void onSetTempoAt(int tick, double tempo);
    // Removes the point exactly at tick; the tick 0 anchor is refused.
    void onRemoveTempoAt(int tick);
    // Inserts a time signature point at barIndex, or edits the existing point
    // at that bar; invalid input (denominator not a power of 2, etc.) is a no-op
    void onSetTimeSignatureAt(int barIndex, int numerator, int denominator);
    // Removes the point at exactly barIndex; the bar 0 anchor is refused
    void onRemoveTimeSignatureAt(int barIndex);
    static void editMasterControl(const TrackControl &control);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                           const QString &redoActionName);

private:
    Q_DECLARE_PRIVATE(AppController)
    // QScopedPointer<AppControllerPrivate> d_ptr;
    AppControllerPrivate *d_ptr;
};

#endif // APPCONTROLLER_H
