#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include <QWidget>

#include "Global/AppOptionsGlobal.h"

class IOptionPage;
class QListWidget;
class QStackedWidget;

// Options settings panel shared by both hosting modes:
//  - Embedded (default): serves as the EmbeddedModalHost content; there is no
//    title bar — the modal is closed by clicking the backdrop or pressing Esc.
//  - Standalone: showStandaloneDialog() wraps the panel in a Dialog (title
//    bar, modal exec) for the classic floating options window.
class AppOptionsDialog : public QWidget {
    Q_OBJECT

public:
    explicit AppOptionsDialog(QWidget *parent = nullptr, bool standalone = false);

    // Switches to the requested settings page (matching the old menu item
    // semantics: General -> page 1, ...).
    void selectOption(AppOptionsGlobal::Option option);

    // Shows the options as a standalone modal dialog (Dialog base + title bar)
    // and enters its modal event loop. Returns the dialog's result.
    static int showStandaloneDialog(AppOptionsGlobal::Option option,
                                    QWidget *parent = nullptr);

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
    bool m_standalone = false;

    QList<IOptionPage *> m_pages;
};

#endif // APPOPTIONSDIALOG_H
