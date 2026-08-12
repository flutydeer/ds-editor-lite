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

    void enqueue(const SingleInstanceRequest &request);

private:
    void dispatchNext();
    void activateWindow();

    DocumentWorkflowController *m_workflow;
    QWidget *m_window;
    QQueue<QString> m_paths;
    bool m_activatePending = false;
};

#endif // EXTERNALOPENREQUESTQUEUE_H
