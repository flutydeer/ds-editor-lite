#ifndef EDITORAPPLICATION_H
#define EDITORAPPLICATION_H

#include <QApplication>
#include <lite/Support/ObjectContext.h>

template <class Application>
class DocumentApplication : public Application {
public:
    using Application::Application;

    bool notify(QObject *receiver, QEvent *event) override {
        // A nested dialog event loop may deliver events to another document.
        // Bind dispatch to the receiver and restore the caller on return.
        const SingletonRegistry::Scope scope(ObjectContext::of(receiver));
        return Application::notify(receiver, event);
    }
};

using EditorApplication = DocumentApplication<QApplication>;
using HeadlessApplication = DocumentApplication<QCoreApplication>;

#endif
