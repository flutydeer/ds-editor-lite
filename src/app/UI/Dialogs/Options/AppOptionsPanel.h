#ifndef APPOPTIONSPANEL_H
#define APPOPTIONSPANEL_H

#include <QWidget>

#include "Global/AppOptionsGlobal.h"

class AudioPage;
class MidiPage;
class AppearancePage;
class GeneralPage;
class InferencePage;
class DeveloperPage;
class IOptionPage;
class QListWidget;
class QStackedWidget;

// Embedded settings panel: serves as the EmbeddedModalHost content and hosts
// all the pages previously owned by AppOptionsDialog; it is no longer a
// top-level window (the old dialog class is kept unused). There is no title
// bar — the modal is closed by clicking the backdrop or pressing Esc.
class AppOptionsPanel : public QWidget {
    Q_OBJECT

public:
    explicit AppOptionsPanel(QWidget *parent = nullptr);

    // Switches to the requested settings page (matching the old menu item
    // semantics: General -> page 1, ...).
    void selectOption(AppOptionsGlobal::Option option);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onSelectionChanged(int index) const;

private:
    void retranslateUi();

    QListWidget *m_tabList = nullptr;
    QStackedWidget *m_pageContent = nullptr;

    GeneralPage *m_generalPage = nullptr;
    AudioPage *m_audioPage = nullptr;
    MidiPage *m_midiPage = nullptr;
    AppearancePage *m_appearancePage = nullptr;
    InferencePage *m_inferencePage = nullptr;
    DeveloperPage *m_developerPage = nullptr;
    QList<IOptionPage *> m_pages;
};

#endif // APPOPTIONSPANEL_H