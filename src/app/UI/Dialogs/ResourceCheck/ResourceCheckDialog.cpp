#include "ResourceCheckDialog.h"

#include "IResourceCheckPage.h"
#include "UI/Controls/AccentButton.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>

ResourceCheckDialog::ResourceCheckDialog(QWidget *parent) : Dialog(parent) {
    setWindowTitle(tr("Check Project Resources"));
    setTitle(tr("Some resources are missing"));
    setMessage(tr("The following resources could not be found on this device. You can relink "
                  "them now, or close this dialog and relink from the clip context menu later."));
    setMinimumSize(720, 400);
    setModal(true);

    m_pageList = new QListWidget;
    m_pageList->setFixedWidth(160);
    m_pageStack = new QStackedWidget;
    connect(m_pageList, &QListWidget::currentRowChanged, m_pageStack,
            &QStackedWidget::setCurrentIndex);

    const auto btnClose = new AccentButton(tr("Close"));
    connect(btnClose, &Button::clicked, this, &Dialog::accept);
    setPositiveButton(btnClose);
}

void ResourceCheckDialog::addPage(IResourceCheckPage *page) {
    m_pages.append(page);
    m_pageStack->addWidget(page);
    m_pageList->addItem(page->title());
}

void ResourceCheckDialog::finalizePages() {
    const auto layout = new QHBoxLayout;
    if (m_pages.count() > 1)
        layout->addWidget(m_pageList);
    else
        m_pageList->hide();
    layout->addWidget(m_pageStack);
    layout->setContentsMargins({});
    body()->setLayout(layout);
    if (m_pageList->count() > 0)
        m_pageList->setCurrentRow(0);
}
