#include "G2pPage.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>

#include "UI/Controls/ComboBox.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"

#include <QPushButton>
#include <language-manager/IG2pManager.h>

G2pPage::G2pPage(QWidget *parent) : IOptionPage(parent) {
    const auto mainLayout = new QVBoxLayout();

    const auto langCard = new OptionsCard();
    langCard->setTitle(tr("G2p"));

    const auto centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(10, 10, 10, 10);

    const auto langListLayout = new QVBoxLayout();
    const auto labelLayout = new QHBoxLayout();
    const auto langLabel = new QLabel(tr("Language Priority"));
    const auto btn_info = new QPushButton();
    btn_info->setStyleSheet("QPushButton {border-radius: 12px;};");
    btn_info->setFixedSize(24, 24);
    btn_info->setIcon(QIcon(":/svg/icons/info_16_filled_white.svg"));
    btn_info->setToolTip(tr("Swap entries can be dragged to divide words in priority order."));
    m_g2pListWidget = new LangSetting::G2pListWidget();
    m_g2pListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    labelLayout->addWidget(langLabel);
    labelLayout->addWidget(btn_info);
    langListLayout->addLayout(labelLayout);
    langListLayout->addWidget(m_g2pListWidget);
    const auto diverLine = new DividerLine();
    diverLine->setOrientation(Qt::Vertical);
    m_g2pInfoWidget = new LangSetting::G2pInfoWidget();

    centerLayout->addLayout(langListLayout, 1);
    centerLayout->addWidget(diverLine);
    centerLayout->addWidget(m_g2pInfoWidget, 2);

    langCard->card()->setLayout(centerLayout);
    mainLayout->addWidget(langCard);
    mainLayout->setContentsMargins({});
    mainLayout->addStretch();

    setLayout(mainLayout);

    m_g2pListWidget->setCurrentIndex(m_g2pListWidget->model()->index(0, 0));

    const auto g2pId = LangMgr::IG2pManager::instance()->g2ps().first()->id();
    m_g2pInfoWidget->setInfo(g2pId);

    connect(m_g2pListWidget, &QListWidget::currentRowChanged, m_g2pInfoWidget, [this] {
        m_g2pInfoWidget->setInfo(m_g2pListWidget->currentItem()->data(Qt::UserRole).toString());
    });

    connect(m_g2pListWidget, &LangSetting::G2pListWidget::shown, m_g2pListWidget, [this] {
        m_g2pInfoWidget->setInfo(m_g2pListWidget->currentItem()->data(Qt::UserRole).toString());
    });

    connect(m_g2pInfoWidget, &LangSetting::G2pInfoWidget::g2pConfigChanged, this,
            &G2pPage::modifyOption);
}

void G2pPage::update() const {
    m_g2pInfoWidget->setInfo(m_g2pListWidget->currentItem()->data(Qt::UserRole).toString());
}

void G2pPage::modifyOption() {
    const auto options = appOptions->language();
    const auto g2pId = m_g2pListWidget->currentItem()->data(Qt::UserRole).toString();
    options->g2pConfigs[g2pId] = LangMgr::IG2pManager::instance()->g2p(g2pId)->config();
    appOptions->saveAndNotify();
}