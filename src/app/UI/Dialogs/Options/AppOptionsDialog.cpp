//
// Created by fluty on 24-3-16.
//

#include "AppOptionsDialog.h"

#include <QStackedWidget>
#include <QScrollArea>

#include "Pages/AppearancePage.h"
#include "Pages/GeneralPage.h"
#include "Pages/G2pPage.h"
#include "Pages/AudioPage.h"
#include "Pages/MidiPage.h"
#include "Pages/InferencePage.h"

AppOptionsDialog::AppOptionsDialog(const AppOptionsGlobal::Option option, QWidget *parent)
    : Dialog(parent) {
    setModal(true);
    setFocusPolicy(Qt::ClickFocus);
    // setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(tr("Options"));

    tabList = new QListWidget;
    tabList->setFixedWidth(160);
    tabList->setObjectName("AppOptionsDialogTabListWidget");
    tabList->addItems(pageNames);
    tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    generalPage = new GeneralPage;
    audioPage = new AudioPage;
    midiPage = new MidiPage;
    // m_pseudoSingerPage = new PseudoSingerPage;
    appearancePage = new AppearancePage;
    g2pPage = new G2pPage;
    inferencePage = new InferencePage;

    pageContent = new QStackedWidget;
    pageContent->addWidget(generalPage);
    pageContent->addWidget(audioPage);
    pageContent->addWidget(midiPage);
    // m_PageContent->addWidget(m_pseudoSingerPage);
    pageContent->addWidget(appearancePage);
    pageContent->addWidget(g2pPage);
    pageContent->addWidget(inferencePage);
    pageContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    pageContent->setMinimumWidth(600);

    pages.append(generalPage);
    pages.append(audioPage);
    pages.append(midiPage);
    // m_pages.append(m_pseudoSingerPage);
    pages.append(appearancePage);
    pages.append(g2pPage);
    pages.append(inferencePage);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(tabList);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(pageContent);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);

    connect(tabList, &QListWidget::currentRowChanged, this,
            &AppOptionsDialog::onSelectionChanged);
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