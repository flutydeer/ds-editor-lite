#ifndef DS_EDITOR_LITE_PROJECTLOADSESSIONBASE_H
#define DS_EDITOR_LITE_PROJECTLOADSESSIONBASE_H

#include "IProjectLoadSession.h"

class Task;
class TaskStatus;

// Shared lifecycle for project load sessions. Owns the start / cancel /
// terminal state machine, the main parse task and the optional
// generation-gated reprocess task (config-change re-parsing), and the
// PreparedProject hand-off. Subclasses provide the format-specific pieces
// through the template-method hooks below; the remaining per-format code
// (configuration pages, materialization) stays in the subclass.
class ProjectLoadSessionBase : public IProjectLoadSession {
    Q_OBJECT

public:
    ProjectLoadSessionBase(QString filePath, quint64 requestId, QObject *parent = nullptr);
    ~ProjectLoadSessionBase() override;

    void start() override;
    void cancel() override;
    PreparedProject takeResult() override;
    [[nodiscard]] quint64 requestId() const override;

protected:
    // --- template-method hooks (format-specific) ---
    virtual void onStart() = 0;                     // initial flow (e.g. package readiness wait)
    virtual Task *createParseTask() = 0;            // main parse task factory
    virtual void handleParseResult(Task *task) = 0; // after a successful parse

    virtual Task *createReprocessTask() {
        return nullptr;
    } // nullptr = not supported

    virtual void handleReprocessResult(Task *task) {
    } // apply re-parse result

    virtual bool shouldPublishProgress() const {
        return false;
    }

    virtual void onCancel() {
    }

    // --- helpers for subclasses ---
    void startParseTask();
    void requestReprocess(); // generation-gated; requires createReprocessTask()
    void detachTask();
    void detachReprocessTask();
    void publishProgress(const TaskStatus &status);
    void finishWithResult(PreparedProject result);
    void fail(const ProjectOperationError &error);
    void emitCanceled();

    [[nodiscard]] bool isTerminal() const {
        return m_terminal;
    }

    QString m_filePath;
    quint64 m_requestId = 0;
    Task *m_task = nullptr;
    Task *m_reprocessTask = nullptr;
    quint64 m_reprocessGeneration = 0;
    PreparedProject m_result;
    bool m_started = false;
    bool m_terminal = false;

private:
    void handleTaskFinished(Task *task);
    void handleReprocessFinished(quint64 generation, Task *task);
};

#endif // DS_EDITOR_LITE_PROJECTLOADSESSIONBASE_H
