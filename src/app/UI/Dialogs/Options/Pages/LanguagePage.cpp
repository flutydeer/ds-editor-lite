#include "LanguagePage.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>

#include "UI/Controls/ComboBox.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"

#include "Modules/Language/LangMgr/ILanguageManager.h"

LanguagePage::LanguagePage(QWidget *parent) : IOptionPage(parent) {
    const auto mainLayout = new QVBoxLayout();

    const auto langCard = new OptionsCard();
    langCard->setTitle(tr("Language"));

    const auto centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(5, 5, 5, 5);

    const auto langListLayout = new QVBoxLayout();
    const auto langLabel = new QLabel(tr("Language Priority"));
    m_langListWidget = new LangMgr::LangListWidget();
    langListLayout->addWidget(langLabel);
    langListLayout->addWidget(m_langListWidget);
    const auto diverLine = new DividerLine();
    diverLine->setOrientation(Qt::Vertical);
    m_langInfoWidget = new LangMgr::LangInfoWidget();
    const auto dividerLine2 = new DividerLine();
    dividerLine2->setOrientation(Qt::Vertical);
    m_g2pInfoWidget = new LangMgr::G2pInfoWidget();

    centerLayout->addLayout(langListLayout, 1);
    centerLayout->addWidget(diverLine);
    centerLayout->addWidget(m_langInfoWidget, 2);
    centerLayout->addWidget(dividerLine2);
    centerLayout->addWidget(m_g2pInfoWidget, 2);

    langCard->card()->setLayout(centerLayout);
    mainLayout->addWidget(langCard);
    mainLayout->addStretch();

    setLayout(mainLayout);

    m_langListWidget->setCurrentIndex(m_langListWidget->model()->index(0, 0));

    const auto langId = LangMgr::ILanguageManager::instance()->languageOrder().first();
    m_langInfoWidget->setInfo(langId);

    const auto g2pId = LangMgr::ILanguageManager::instance()->language(langId)->selectedG2p();
    m_g2pInfoWidget->setInfo(langId, g2pId);

    connect(m_langListWidget, &QListWidget::currentRowChanged, m_langInfoWidget, [this] {
        m_langInfoWidget->setInfo(m_langListWidget->currentItem()->data(Qt::UserRole).toString());
        modifyOption();
    });

    connect(m_langInfoWidget, &LangMgr::LangInfoWidget::g2pSelected, m_g2pInfoWidget,
            &LangMgr::G2pInfoWidget::setInfo);

    connect(m_langInfoWidget, &LangMgr::LangInfoWidget::langConfigChanged, this,
            &LanguagePage::modifyOption);

    connect(m_g2pInfoWidget, &LangMgr::G2pInfoWidget::g2pConfigChanged, this,
            &LanguagePage::modifyOption);
}

void LanguagePage::modifyOption() {
    const auto options = AppOptions::instance()->language();
    options->langOrder = LangMgr::ILanguageManager::instance()->languageOrder();
    const auto langId = m_langListWidget->currentItem()->data(Qt::UserRole).toString();
    options->langConfigs[langId] =
        LangMgr::ILanguageManager::instance()->language(langId)->exportConfig();
    AppOptions::instance()->saveAndNotify();
}