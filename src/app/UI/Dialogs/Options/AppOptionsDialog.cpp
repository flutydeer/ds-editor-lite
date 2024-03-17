//
// Created by fluty on 24-3-16.
//

#include "AppOptionsDialog.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>

#include "Controller/AppOptionsController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Pages/AppearancePage.h"
#include "Pages/GeneralPage.h"
#include "UI/Controls/Button.h"
#include "Pages/AudioPage.h"

AppOptionsDialog::AppOptionsDialog(Page page, QWidget *parent) : OKCancelApplyDialog(parent) {

    connect(okButton(), &Button::clicked, this, &AppOptionsDialog::accept);
    connect(this, &AppOptionsDialog::accepted, this, &AppOptionsDialog::apply);
    connect(cancelButton(), &Button::clicked, this, &AppOptionsDialog::cancel);
    connect(applyButton(), &Button::clicked, this, &AppOptionsDialog::apply);

    m_tabList = new QListWidget;
    m_tabList->setFixedWidth(160);
    m_tabList->setObjectName("AppOptionsDialogTabListWidget");
    m_tabList->addItems(m_pageNames);

    m_generalPage = new GeneralPage;
    m_audioPage = new AudioPage;
    m_appearancePage = new AppearancePage;

    m_PageArea = new QStackedWidget;
    m_PageArea->addWidget(m_generalPage);
    m_PageArea->addWidget(m_audioPage);
    m_PageArea->addWidget(m_appearancePage);

    m_pages.append(m_generalPage);
    m_pages.append(m_audioPage);
    m_pages.append(m_appearancePage);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_tabList);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(m_PageArea);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);

    connect(m_tabList, &QListWidget::currentRowChanged, this,
            &AppOptionsDialog::onSelectionChanged);
    m_tabList->setCurrentRow(page);

    resize(900, 600);
}
void AppOptionsDialog::apply() {
    AppOptionsController::instance()->apply();
}
void AppOptionsDialog::cancel() {
    AppOptionsController::instance()->cancel();
    reject();
}
void AppOptionsDialog::onSelectionChanged(int index) {
    m_PageArea->setCurrentWidget(m_pages.at(index));
}