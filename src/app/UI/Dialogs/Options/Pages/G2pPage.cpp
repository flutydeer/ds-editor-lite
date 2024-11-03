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
#include <language-manager/ILanguageManager.h>

G2pPage::G2pPage(QWidget *parent) : IOptionPage(parent) {
    const auto mainLayout = new QVBoxLayout();

    const auto langCard = new OptionsCard();
    langCard->setTitle(tr("G2p"));

    const auto centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(10, 10, 10, 10);

    m_g2pListWidget = new LangSetting::G2pListWidget();
    const auto diverLine = new DividerLine();
    diverLine->setOrientation(Qt::Vertical);
    m_g2pInfoWidget = new LangSetting::G2pInfoWidget();

    centerLayout->addWidget(m_g2pListWidget, 1);
    centerLayout->addWidget(diverLine);
    centerLayout->addWidget(m_g2pInfoWidget, 2);

    langCard->card()->setLayout(centerLayout);
    mainLayout->addWidget(langCard);
    mainLayout->setContentsMargins({});
    mainLayout->addStretch();

    setLayout(mainLayout);

    // 更新删除按钮状态、g2p
    connect(m_g2pListWidget->m_gListWidget, &QListWidget::currentRowChanged, m_g2pListWidget,
            [this] { m_g2pListWidget->m_gListWidget->updateDeleteButtonState(); });
    connect(m_g2pListWidget->m_gListWidget, &QListWidget::currentRowChanged, m_g2pInfoWidget,
            [this] { m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId()); });

    connect(m_g2pListWidget->m_gListWidget, &LangSetting::GListWidget::shown,
            m_g2pListWidget->m_gListWidget,
            [this] { m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId()); });

    connect(m_g2pInfoWidget, &LangSetting::G2pInfoWidget::g2pConfigChanged, this,
            &G2pPage::modifyOption);

    m_g2pListWidget->m_gListWidget->setCurrentIndex(
        m_g2pListWidget->m_gListWidget->model()->index(0, 0));
}

void G2pPage::update() const {
    m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId());
}

void G2pPage::modifyOption() {
    const auto options = appOptions->language();
    const auto g2pId = m_g2pListWidget->m_gListWidget->currentItem()->data(Qt::UserRole).toString();
    const auto g2pConfig = LangMgr::ILanguageManager::instance()->g2p(g2pId)->config();
    if (!g2pConfig.empty())
        options->g2pConfigs.insert(g2pId, g2pConfig);
    appOptions->saveAndNotify();
}