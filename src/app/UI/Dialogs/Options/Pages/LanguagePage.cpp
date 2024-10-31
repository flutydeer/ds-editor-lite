#include "LanguagePage.h"

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
#include <language-manager/ILanguageManager.h>

LanguagePage::LanguagePage(QWidget *parent) : IOptionPage(parent) {
    const auto mainLayout = new QVBoxLayout();

    const auto langCard = new OptionsCard();
    langCard->setTitle(tr("Language"));

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
    m_langListWidget = new LangSetting::LangListWidget();
    m_langListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    labelLayout->addWidget(langLabel);
    labelLayout->addWidget(btn_info);
    langListLayout->addLayout(labelLayout);
    langListLayout->addWidget(m_langListWidget);
    const auto diverLine = new DividerLine();
    diverLine->setOrientation(Qt::Vertical);
    m_langInfoWidget = new LangSetting::LangInfoWidget();
    const auto dividerLine2 = new DividerLine();
    dividerLine2->setOrientation(Qt::Vertical);
    m_g2pInfoWidget = new LangSetting::G2pInfoWidget();

    centerLayout->addLayout(langListLayout, 1);
    centerLayout->addWidget(diverLine);
    centerLayout->addWidget(m_langInfoWidget, 2);
    centerLayout->addWidget(dividerLine2);
    centerLayout->addWidget(m_g2pInfoWidget, 2);

    langCard->card()->setLayout(centerLayout);
    mainLayout->addWidget(langCard);
    mainLayout->setContentsMargins({});
    mainLayout->addStretch();

    setLayout(mainLayout);

    m_langListWidget->setCurrentIndex(m_langListWidget->model()->index(0, 0));

    const auto langId = LangMgr::ILanguageManager::instance()->defaultOrder().first();
    m_langInfoWidget->setInfo(langId);

    const auto g2pId = LangMgr::ILanguageManager::instance()->language(langId)->selectedG2p();
    m_g2pInfoWidget->setInfo(g2pId);

    connect(m_langListWidget, &QListWidget::currentRowChanged, m_langInfoWidget, [this] {
        m_langInfoWidget->setInfo(m_langListWidget->currentItem()->data(Qt::UserRole).toString());
    });

    connect(m_langListWidget, &LangSetting::LangListWidget::shown, m_langInfoWidget, [this] {
        m_langInfoWidget->setInfo(m_langListWidget->currentItem()->data(Qt::UserRole).toString());
    });

    connect(m_langInfoWidget, &LangSetting::LangInfoWidget::g2pSelected, m_g2pInfoWidget,
            &LangSetting::G2pInfoWidget::setInfo);

    connect(m_langListWidget, &LangSetting::LangListWidget::priorityChanged, this,
            &LanguagePage::modifyOption);

    connect(m_langInfoWidget, &LangSetting::LangInfoWidget::langConfigChanged, this,
            &LanguagePage::modifyOption);

    connect(m_g2pInfoWidget, &LangSetting::G2pInfoWidget::g2pConfigChanged, this,
            &LanguagePage::modifyOption);
}

void LanguagePage::modifyOption() {
    const auto options = appOptions->language();
    options->langOrder = LangMgr::ILanguageManager::instance()->defaultOrder();
    const auto langId = m_langListWidget->currentItem()->data(Qt::UserRole).toString();
    options->langConfigs[langId] =
        LangMgr::ILanguageManager::instance()->language(langId)->exportConfig();
    appOptions->saveAndNotify();
}