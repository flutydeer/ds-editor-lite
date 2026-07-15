#include "G2pPage.h"

#include <QLabel>
#include <QVBoxLayout>

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"

G2pPage::G2pPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void G2pPage::update() const {
    m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId());
}

void G2pPage::modifyOption() {
    appOptions->saveAndNotify(AppOptionsGlobal::G2pLanguage);
}

QWidget *G2pPage::createContentWidget() {
    const auto widget = new QWidget;
    const auto mainLayout = new QVBoxLayout();

    // 语言引擎状态概览行。
    // 复用 appStatus->languageModuleStatus 与 moduleStatusChanged 信号，无需新管线。
    // R6/TD-8: Error 时通过 toolTip 展示 languageModuleError 的具体原因。
    const auto engineStatusLabel = new QLabel();
    const auto formatEngineStatus = [engineStatusLabel]() {
        const auto status = appStatus->languageModuleStatus;
        const QString error = appStatus->languageModuleError;
        QString text;
        switch (status) {
            case AppStatus::ModuleStatus::Ready:
                text = tr("Language engine: Ready");
                break;
            case AppStatus::ModuleStatus::Loading:
                text = tr("Language engine: Loading...");
                break;
            case AppStatus::ModuleStatus::Error:
                text = tr("Language engine: Error (voicebank G2P may be unavailable)");
                break;
            case AppStatus::ModuleStatus::Unknown:
                text = tr("Language engine: Not started");
                break;
        }
        engineStatusLabel->setText(text);
        // Error 详情通过 toolTip 展示，避免长文本破坏布局
        engineStatusLabel->setToolTip(error.isEmpty() ? QString() : error);
    };
    formatEngineStatus();
    connect(appStatus, &AppStatus::moduleStatusChanged, engineStatusLabel,
            [formatEngineStatus](AppStatus::ModuleType module, AppStatus::ModuleStatus) {
                if (module == AppStatus::ModuleType::Language)
                    formatEngineStatus();
            });
    connect(appStatus, &AppStatus::languageModuleErrorChanged, engineStatusLabel,
            [formatEngineStatus](const QString &) { formatEngineStatus(); });
    mainLayout->addWidget(engineStatusLabel);

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

    widget->setLayout(mainLayout);

    // 更新删除按钮状态、g2p
    connect(m_g2pListWidget->m_gListWidget, &QListWidget::currentRowChanged, m_g2pListWidget,
            [this] { m_g2pListWidget->m_gListWidget->updateDeleteButtonState(); });
    connect(m_g2pListWidget->m_gListWidget, &QListWidget::currentRowChanged, m_g2pInfoWidget,
            [this] { m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId()); });

    connect(m_g2pListWidget->m_gListWidget, &LangSetting::GListWidget::shown,
            m_g2pListWidget->m_gListWidget,
            [this] { m_g2pInfoWidget->setInfo(m_g2pListWidget->currentG2pId()); });

    m_g2pListWidget->m_gListWidget->setCurrentIndex(
        m_g2pListWidget->m_gListWidget->model()->index(0, 0));

    return widget;
}
