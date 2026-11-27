#ifndef EXTERNALOPENREQUESTQUEUE_H
#define EXTERNALOPENREQUESTQUEUE_H

#include "SingleInstanceProtocol.h"

#include <QObject>

class DocumentWindowManager;

class ExternalOpenRequestQueue final : public QObject {
public:
    explicit ExternalOpenRequestQueue(DocumentWindowManager &windows);

    // Returns whether the main thread actually admitted the request into its
    // bounded work queue. A rejection is propagated back to the secondary
    // instead of being silently dropped after an early ACK.
    AdmissionResult take(const SingleInstanceRequest &request);

private:
    DocumentWindowManager &m_windows;
    qsizetype m_queuedPaths = 0;
};

#endif // EXTERNALOPENREQUESTQUEUE_H
