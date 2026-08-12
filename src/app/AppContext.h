#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <lite/Core/SingletonRegistry.h>

#include <QUuid>

class ApplicationContext;
class DocumentSession;
class QObject;

// Transitional lookup facade. ApplicationContext owns process-wide services;
// DocumentSession owns document services and activates them in SingletonRegistry.
class AppContext {
public:
    template <typename T>
    static T *instance() {
        return SingletonRegistry::instance<T>();
    }

    static ApplicationContext *application();
    static DocumentSession *currentSession();
    static QUuid currentDocumentId();
    static void activateSession(DocumentSession *session);

private:
    friend class ApplicationContext;
    static void setApplication(ApplicationContext *application);

    static ApplicationContext *s_application;
    static DocumentSession *s_currentSession;
};

#endif // APPCONTEXT_H
