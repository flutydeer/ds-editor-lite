#include "AppOptionsPanel.h"

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

AppOptionsPanel::AppOptionsPanel(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("AppOptionsPanel"));
    setFocusPolicy(Qt::ClickFocus);

    m_tabList = new QListWidget;
    m_tabList->setFixedWidth(160);
    m_tabList->setObjectName("AppOptionsDialogTabListWidget");
    m_tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

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
    body->setContentsMargins(12, 12, 12, 12);
    const auto bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(m_tabList);
    bodyLayout->addSpacing(12);
    bodyLayout->addWidget(m_pageContent);
    bodyLayout->setContentsMargins({});
    body->setLayout(bodyLayout);

    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    mainLayout->addWidget(body);

    connect(m_tabList, &QListWidget::currentRowChanged, this, &AppOptionsPanel::onSelectionChanged);
}

void AppOptionsPanel::onSelectionChanged(const int index) const {
    m_pageContent->setCurrentWidget(m_pages.at(index));
}

void AppOptionsPanel::selectOption(const AppOptionsGlobal::Option option) {
    const auto pageIndex = option >= 1 ? option - 1 : 0; // Skip enum "All"
    m_tabList->setCurrentRow(pageIndex);
}

void AppOptionsPanel::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void AppOptionsPanel::retranslateUi() {
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