#include "AppContext.h"

#include "ApplicationContext.h"
#include "DocumentSession.h"

ApplicationContext *AppContext::s_application = nullptr;
DocumentSession *AppContext::s_currentSession = nullptr;

ApplicationContext *AppContext::application() {
    return s_application;
}

DocumentSession *AppContext::currentSession() {
    return s_currentSession;
}

QUuid AppContext::currentDocumentId() {
    return s_currentSession ? s_currentSession->id() : QUuid{};
}

void AppContext::activateSession(DocumentSession *session) {
    if (s_currentSession && s_currentSession != session)
        s_currentSession->unregisterServices();
    s_currentSession = session;
    if (session)
        session->registerServices();
}

void AppContext::setApplication(ApplicationContext *application) {
    s_application = application;
}
