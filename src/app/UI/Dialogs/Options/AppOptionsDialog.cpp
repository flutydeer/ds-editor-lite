#include "AppOptionsDialog.h"

#include <QStackedWidget>
#include <QScrollArea>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QSignalBlocker>

#include "Pages/AppearancePage.h"
#include "Pages/GeneralPage.h"
// #include "Pages/G2pPage.h"
#include "Pages/AudioPage.h"
#include "Pages/MidiPage.h"
#include "Pages/InferencePage.h"
#include "Pages/DeveloperPage.h"

AppOptionsDialog::AppOptionsDialog(const AppOptionsGlobal::Option option, QWidget *parent)
    : Dialog(parent) {
    setModal(true);
    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("Options"));

    tabList = new QListWidget;
    tabList->setFixedWidth(160);
    tabList->setObjectName("AppOptionsDialogTabListWidget");
    tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    generalPage = new GeneralPage;
    audioPage = new AudioPage;
    midiPage = new MidiPage;
    appearancePage = new AppearancePage;
    inferencePage = new InferencePage;
    developerPage = new DeveloperPage;

    pageContent = new QStackedWidget;
    pageContent->addWidget(generalPage);
    pageContent->addWidget(audioPage);
    pageContent->addWidget(midiPage);
    pageContent->addWidget(appearancePage);
    pageContent->addWidget(inferencePage);
    pageContent->addWidget(developerPage);
    pageContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    pageContent->setMinimumWidth(600);

    pages.append(generalPage);
    pages.append(audioPage);
    pages.append(midiPage);
    pages.append(appearancePage);
    pages.append(inferencePage);
    pages.append(developerPage);

    retranslateUi();

    // Same sidebar container treatment as the embedded panel: the sidebar
    // owns its background (QWidget#AppOptionsSidebar) and inner margins; the
    // list itself stays transparent and unpadded.
    const auto sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("AppOptionsSidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground);
    // Standalone mode: flush against the square window edge, so the QSS
    // drops the left rounding (QWidget#AppOptionsSidebar[standalone="true"]).
    sidebar->setProperty("standalone", true);
    const auto sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 9, 12, 9);
    sidebarLayout->addWidget(tabList);
    sidebar->setFixedWidth(160 + sidebarLayout->contentsMargins().left()
                           + sidebarLayout->contentsMargins().right());

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(pageContent);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    // The Dialog base leaves a 12px inset around the body; the options layout
    // is flush by design (sidebar owns its background, pages carry their own
    // 16px content margins), so drop it here.
    body()->setContentsMargins({});
    body()->setLayout(mainLayout);

    connect(tabList, &QListWidget::currentRowChanged, this, &AppOptionsDialog::onSelectionChanged);
    const auto pageIndex = option >= 1 ? option - 1 : 0; // Skip enum "All"
    tabList->setCurrentRow(pageIndex);

    resize(900, 600);
}

AppOptionsDialog::~AppOptionsDialog() {
    qDebug() << "dispose";
}

void AppOptionsDialog::onSelectionChanged(const int index) const {
    pageContent->setCurrentWidget(pages.at(index));
}

void AppOptionsDialog::changeEvent(QEvent *event) {
    Dialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void AppOptionsDialog::retranslateUi() {
    setWindowTitle(tr("Options"));

    const QStringList pageNames = {tr("General"),    tr("Audio"),     tr("MIDI"),
                                   tr("Appearance"), tr("Inference"), tr("Developer Options")};
    const QSignalBlocker blocker(tabList);
    const auto currentRow = tabList->currentRow();
    if (tabList->count() != pageNames.size()) {
        tabList->clear();
        tabList->addItems(pageNames);
    } else {
        for (int i = 0; i < pageNames.size(); ++i)
            tabList->item(i)->setText(pageNames.at(i));
    }
    tabList->setCurrentRow(currentRow);
}
