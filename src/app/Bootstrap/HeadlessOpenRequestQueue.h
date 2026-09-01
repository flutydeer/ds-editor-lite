#ifndef HEADLESSOPENREQUESTQUEUE_H
#define HEADLESSOPENREQUESTQUEUE_H

#include "Automation/Public/PublicAutomationRegistry.h"
#include "SingleInstanceProtocol.h"

#include <QObject>
#include <QQueue>

#include <functional>

namespace Automation {
    class CoreRuntime;
}

class HeadlessOpenRequestQueue final : public QObject {
public:
    using OpenDocument = std::function<Automation::AutomationResult<Automation::TaskAcceptedResult>(
        const Automation::PublicDocumentOpenRequest &)>;

    HeadlessOpenRequestQueue(Automation::CoreRuntime &runtime, OpenDocument openDocument,
                             QObject *parent = nullptr);

    void enqueue(const SingleInstanceRequest &request);

private:
    void dispatchNext();
    void taskFinished(const Automation::AutomationTaskSnapshot &snapshot);

    Automation::CoreRuntime &m_runtime;
    OpenDocument m_openDocument;
    QQueue<QString> m_paths;
    bool m_busy = false;
};

#endif // HEADLESSOPENREQUESTQUEUE_H
