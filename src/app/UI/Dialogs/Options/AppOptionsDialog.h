#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include <QWidget>
#include <QStringList>

#include "Global/AppOptionsGlobal.h"

class IOptionPage;
class QListWidget;
class QStackedWidget;

// Settings panel shared by the standalone options window and embedded hosts.
class AppOptionsDialog : public QWidget {
    Q_OBJECT

public:
    explicit AppOptionsDialog(QWidget *parent = nullptr);

    // Switches to the requested settings page (matching the old menu item
    // semantics: General -> page 1, ...).
    void selectOption(AppOptionsGlobal::Option option);

    void setAutomationCustomPermissionOperationIds(QStringList operationIds);

    static void showStandaloneDialog(AppOptionsGlobal::Option option, QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onSelectionChanged(int index);

private:
    IOptionPage *ensurePage(int index);
    void retranslateUi();

    QListWidget *m_tabList = nullptr;
    QStackedWidget *m_pageContent = nullptr;
    QWidget *m_sidebar = nullptr;

    QList<IOptionPage *> m_pages;
    QStringList m_automationCustomPermissionOperationIds;
};

#endif // APPOPTIONSDIALOG_H
