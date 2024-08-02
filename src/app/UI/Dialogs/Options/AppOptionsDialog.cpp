//
// Created by fluty on 24-3-16.
//

#include "AppOptionsDialog.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QScrollArea>

#include "Model/AppOptions/AppOptions.h"
#include "Pages/AppearancePage.h"
#include "Pages/GeneralPage.h"
#include "Pages/LanguagePage.h"
#include "Pages/AudioPage.h"
#include "Pages/MidiPage.h"

AppOptionsDialog::AppOptionsDialog(Page page, QWidget *parent) : Dialog(parent) {
    setFocusPolicy(Qt::ClickFocus);

    m_tabList = new QListWidget;
    m_tabList->setFixedWidth(160);
    m_tabList->setObjectName("AppOptionsDialogTabListWidget");
    m_tabList->addItems(m_pageNames);

    m_generalPage = new GeneralPage;
    m_audioPage = new AudioPage;
    m_midiPage = new MidiPage;
    m_appearancePage = new AppearancePage;
    m_languagePage = new LanguagePage;

    m_PageContent = new QStackedWidget;
    m_PageContent->addWidget(m_generalPage);
    m_PageContent->addWidget(m_audioPage);
    m_PageContent->addWidget(m_midiPage);
    m_PageContent->addWidget(m_appearancePage);
    m_PageContent->addWidget(m_languagePage);
    m_PageContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);

    auto pageScrollArea = new QScrollArea;
    pageScrollArea->setWidget(m_PageContent);
    pageScrollArea->setWidgetResizable(true);

    m_pages.append(m_generalPage);
    m_pages.append(m_audioPage);
    m_pages.append(m_midiPage);
    m_pages.append(m_appearancePage);
    m_pages.append(m_languagePage);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_tabList);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(pageScrollArea);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);

    connect(m_tabList, &QListWidget::currentRowChanged, this,
            &AppOptionsDialog::onSelectionChanged);
    m_tabList->setCurrentRow(page);

    resize(900, 600);
}
void AppOptionsDialog::onSelectionChanged(int index) {
    m_PageContent->setCurrentWidget(m_pages.at(index));
}