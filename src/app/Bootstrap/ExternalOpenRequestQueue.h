#ifndef EXTERNALOPENREQUESTQUEUE_H
#define EXTERNALOPENREQUESTQUEUE_H

#include "SingleInstanceProtocol.h"

#include <QObject>
#include <QQueue>

class DocumentWorkflowController;
class QWidget;

class ExternalOpenRequestQueue final : public QObject {
public:
    ExternalOpenRequestQueue(DocumentWorkflowController *workflow, QWidget *window,
                             QObject *parent = nullptr);

    // Returns whether the main thread actually admitted the request into its
    // bounded work queue. A rejection is propagated back to the secondary
    // instead of being silently dropped after an early ACK.
    // localStartup marks the request created from this process's own command
    // line: the external in-flight cap must not discard project paths the user
    // explicitly passed at launch.
    AdmissionResult take(const SingleInstanceRequest &request, bool localStartup = false);

private:
    void dispatchNext();
    void activateWindow();

    DocumentWorkflowController *m_workflow;
    QWidget *m_window;
    QQueue<QString> m_paths;
    bool m_activatePending = false;
};

#endif // EXTERNALOPENREQUESTQUEUE_H
