#include "AppOptionsDialog.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Pages/AppearancePage.h"
#include "Pages/AudioPage.h"
#include "Pages/DeveloperPage.h"
#include "Pages/GeneralPage.h"
#include "Pages/InferencePage.h"
#include "Pages/MidiPage.h"
#include "UI/Dialogs/Base/Dialog.h"

AppOptionsDialog::AppOptionsDialog(QWidget *parent, const bool standalone)
    : QWidget(parent), m_standalone(standalone) {
    setObjectName(QStringLiteral("AppOptionsDialog"));
    setFocusPolicy(Qt::ClickFocus);

    m_tabList = new QListWidget;
    m_tabList->setFixedWidth(160);
    m_tabList->setObjectName("AppOptionsDialogTabListWidget");
    m_tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // The sidebar is a dedicated container owning the sidebar background;
    // the list itself stays transparent and unpadded (box-model padding on a
    // scroll area caused a spurious scrollbar).
    m_sidebar = new QWidget;
    m_sidebar->setObjectName(QStringLiteral("AppOptionsSidebar"));
    m_sidebar->setAttribute(Qt::WA_StyledBackground);
    // Standalone mode: flush against the square window edge, so the QSS
    // drops the left rounding (QWidget#AppOptionsSidebar[standalone="true"]).
    m_sidebar->setProperty("standalone", m_standalone);
    const auto sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(12, 9, 12, 9);
    sidebarLayout->addWidget(m_tabList);
    // The list's raw sizeHint (content-based) can exceed its fixed width and
    // would stretch the sidebar; pin the sidebar to list width + margins so
    // the background hugs the list.
    m_sidebar->setFixedWidth(m_tabList->minimumWidth() + sidebarLayout->contentsMargins().left()
                             + sidebarLayout->contentsMargins().right());

    m_generalPage = new GeneralPage;
    m_audioPage = new AudioPage;
    m_midiPage = new MidiPage;
    m_appearancePage = new AppearancePage;
    m_inferencePage = new InferencePage;
    m_developerPage = new DeveloperPage;

    m_pageContent = new QStackedWidget;
    m_pageContent->addWidget(m_generalPage);
    m_pageContent->addWidget(m_audioPage);
    m_pageContent->addWidget(m_midiPage);
    m_pageContent->addWidget(m_appearancePage);
    m_pageContent->addWidget(m_inferencePage);
    m_pageContent->addWidget(m_developerPage);
    m_pageContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    m_pageContent->setMinimumWidth(600);

    m_pages.append(m_generalPage);
    m_pages.append(m_audioPage);
    m_pages.append(m_midiPage);
    m_pages.append(m_appearancePage);
    m_pages.append(m_inferencePage);
    m_pages.append(m_developerPage);

    retranslateUi();

    const auto body = new QWidget;
    body->setContentsMargins({});
    const auto bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(m_sidebar);
    bodyLayout->addWidget(m_pageContent);
    bodyLayout->setContentsMargins({});
    bodyLayout->setSpacing(0);
    body->setLayout(bodyLayout);

    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    mainLayout->addWidget(body);

    connect(m_tabList, &QListWidget::currentRowChanged, this, &AppOptionsDialog::onSelectionChanged);
}

int AppOptionsDialog::showStandaloneDialog(const AppOptionsGlobal::Option option,
                                           QWidget *parent) {
    auto *dialog = new Dialog(parent);
    auto *panel = new AppOptionsDialog(dialog, /*standalone=*/true);
    panel->selectOption(option);

    dialog->setModal(true);
    // The Dialog base leaves a 12px inset around the body; the options layout
    // is flush by design (sidebar owns its background, pages carry their own
    // 16px content margins), so drop it here.
    dialog->body()->setContentsMargins({});
    const auto bodyLayout = new QHBoxLayout(dialog->body());
    bodyLayout->setContentsMargins({});
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(panel);

    dialog->resize(900, 600);
    const auto result = dialog->exec();
    dialog->deleteLater();
    return result;
}

void AppOptionsDialog::onSelectionChanged(const int index) const {
    m_pageContent->setCurrentWidget(m_pages.at(index));
}

void AppOptionsDialog::selectOption(const AppOptionsGlobal::Option option) {
    const auto pageIndex = option >= 1 ? option - 1 : 0; // Skip enum "All"
    m_tabList->setCurrentRow(pageIndex);
}

void AppOptionsDialog::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void AppOptionsDialog::retranslateUi() {
    // In standalone mode the wrapping Dialog (the panel's window) owns the
    // title bar; the title must be set on it so DialogTitleBar picks it up via
    // WindowTitleChange (skipped when embedded).
    if (m_standalone)
        window()->setWindowTitle(tr("Options"));

    const QStringList pageNames = {tr("General"),    tr("Audio"),     tr("MIDI"),
                                   tr("Appearance"), tr("Inference"), tr("Developer Options")};
    const QSignalBlocker blocker(m_tabList);
    const auto currentRow = m_tabList->currentRow();
    if (m_tabList->count() != pageNames.size()) {
        m_tabList->clear();
        m_tabList->addItems(pageNames);
    } else {
        for (int i = 0; i < pageNames.size(); ++i)
            m_tabList->item(i)->setText(pageNames.at(i));
    }
    m_tabList->setCurrentRow(currentRow);
}
