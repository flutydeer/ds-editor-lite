#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#define taskManager TaskManager::instance()

#include <QThreadPool>
#include <QUuid>

#include <functional>

class Task;
class TaskManagerPrivate;

class TaskManager : public QObject {
    Q_OBJECT

public:
    enum TaskChangeType { Added, Removed };

private:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager() override;

public:
    // Leaky Meyers singleton: task runtime below AppContext. AppContext drains
    // it during teardown (terminate/wait) but no longer owns it.
    static TaskManager *instance();
    Q_DISABLE_COPY_MOVE(TaskManager)

public:
    [[nodiscard]] const QList<Task *> &tasks() const;
    Task *findTaskById(int id);
    QList<Task *> tasksForDocument(const QUuid &documentId) const;
    void setDocumentIdProvider(std::function<QUuid()> provider);

signals:
    void allDone();
    void taskChanged(TaskChangeType type, Task *task, qsizetype index);

public slots:
    void addTask(Task *task);
    void startTask(Task *task);
    void addAndStartTask(Task *task);
    void removeTask(Task *task);
    void startAllTasks();
    static void terminateTask(Task *task);
    void terminateAllTasks();
    void terminateTasks(const QUuid &documentId);
    void wait();
    void waitForDocument(const QUuid &documentId);

private slots:
    void onWorkerWaitDone();

private:
    Q_DECLARE_PRIVATE(TaskManager)
    TaskManagerPrivate *d_ptr;
};



#endif // TASKMANAGER_H
