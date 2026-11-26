#pragma once

#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"

#include <QPointer>
#include <list>
#include <memory>

class AppContext;
class MainWindow;
class WindowPlacement;

class DocumentWindowManager final : public QObject {
    Q_OBJECT
public:
    explicit DocumentWindowManager(AppContext &process);
    ~DocumentWindowManager() override;
    static DocumentWindowManager *instance();

    struct Document {
        AppContext *context;
        std::unique_ptr<AppContext> ownedContext;
        std::unique_ptr<MainWindow> window;
        std::unique_ptr<WindowPlacement> placement;
        QString openingPath;
        bool closing = false;
        ~Document();
    };

    Document &createWindow();
    const auto &documents() const { return m_documents; }
    Document *find(const Automation::DocumentId &id) const;
    Document *find(const AppContext *context) const;
    Document *findPath(const QString &path) const;
    Document *active() const;
    void activate(Document &document) const;
    void open(const QString &path, bool reuseBlank = false, Document *replace = nullptr);
    bool close(Document &document, TerminationSavePolicy policy);
    TerminationRequestResult terminate(TerminationMode mode, TerminationSavePolicy policy);
    bool terminating() const { return m_terminating; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool confirmClose(Document &document, TerminationSavePolicy policy,
                      const QString &progress = {});
    void retire(Document &document);
    void releaseRetired(Document *document);
    AppContext &m_process;
    std::list<std::unique_ptr<Document>> m_documents;
    Document *m_active = nullptr;
    bool m_terminating = false;
    int m_nextWindowNumber = 1;
};
