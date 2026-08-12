#include "SessionAwareApplication.h"

#include "DocumentSession.h"

SessionAwareApplication::SessionAwareApplication(int &argc, char **argv)
    : QApplication(argc, argv) {
}

bool SessionAwareApplication::notify(QObject *receiver, QEvent *event) {
    for (auto *cursor = receiver; cursor; cursor = cursor->parent()) {
        if (auto *session = dynamic_cast<DocumentSession *>(cursor)) {
            session->activate();
            break;
        }
        const auto sessionPointer = cursor->property("documentSession");
        if (sessionPointer.isValid()) {
            if (auto *session = dynamic_cast<DocumentSession *>(sessionPointer.value<QObject *>()))
                session->activate();
            break;
        }
    }
    return QApplication::notify(receiver, event);
}
