#ifndef RESOURCECHECKDIALOG_H
#define RESOURCECHECKDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

class IResourceCheckPage;
class QListWidget;
class QStackedWidget;

// Project resource check dialog: one place to handle resource issues that need user decisions after opening a project.
// Multi-page skeleton (one page per resource type); currently audio only. Pages are assembled on demand,
// and navigation is hidden for a single page. Non-blocking: the user can close anytime; unresolved items stay missing
class ResourceCheckDialog final : public Dialog {
    Q_OBJECT

public:
    explicit ResourceCheckDialog(QWidget *parent = nullptr);

    void addPage(IResourceCheckPage *page);
    // Call after all pages are added to build the layout (navigation hidden for a single page)
    void finalizePages();

private:
    QListWidget *m_pageList = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    QList<IResourceCheckPage *> m_pages;
};

#endif // RESOURCECHECKDIALOG_H
