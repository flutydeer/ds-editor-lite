#include "G2pInfoWidget.h"

#include "Utils/VersionUtils.h"

#include <synthrt/G2P/Core/Manager.h>

#include <QLoggingCategory>
#include <QtGlobal>

Q_LOGGING_CATEGORY(logLangSetting, "lang.setting")

namespace LangSetting {
    G2pInfoWidget::G2pInfoWidget(QWidget *parent) : QWidget(parent) {
        this->setContentsMargins(0, 0, 0, 0);

        this->m_mainLayout = new QVBoxLayout();
        this->m_mainLayout->setContentsMargins(0, 0, 0, 0);
        this->m_topLayout = new QVBoxLayout();
        this->m_authorLayout = new QHBoxLayout();

        this->m_label = new QLabel(tr("G2P Config"));

        this->m_languageLabel = new QLabel(tr("Language: "));
        this->m_authorLabel = new QLabel(tr("Author: "));
        this->m_authorLayout->addWidget(this->m_languageLabel);
        this->m_authorLayout->addStretch();
        this->m_authorLayout->addWidget(this->m_authorLabel);
        this->m_authorLayout->addStretch();

        this->m_descriptionGroupBox = new QGroupBox(tr("Description "));
        this->m_descriptionLayout = new QVBoxLayout();
        this->m_descriptionLabel = new QLabel();
        this->m_descriptionLabel->setWordWrap(true);
        this->m_descriptionLayout->addWidget(this->m_descriptionLabel);
        this->m_descriptionLayout->setContentsMargins(0, 0, 0, 0);

        this->m_descriptionGroupBox->setLayout(this->m_descriptionLayout);

        this->m_topLayout->addWidget(this->m_label);
        this->m_topLayout->addLayout(this->m_authorLayout);
        this->m_topLayout->addWidget(this->m_descriptionGroupBox);
        this->m_topLayout->addStretch(1);

        this->m_mainLayout->addLayout(this->m_topLayout, 1);
        this->setLayout(this->m_mainLayout);
    }

    G2pInfoWidget::~G2pInfoWidget() = default;

    void G2pInfoWidget::removeWidget() const {
        QLayoutItem *child;
        while ((child = this->m_mainLayout->takeAt(1)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    void G2pInfoWidget::setInfo(const QString &g2pId) const {
        // R2/TD-2: 列表为空时 currentG2pId() 返回空 QString，显示占位文案而非
        // 走 task 查找（避免误导性的 "Load Failed" 提示）
        if (g2pId.isEmpty()) {
            m_label->setText(tr("G2P Config"));
            m_descriptionLabel->setText(tr("(no G2P preset selected)"));
            return;
        }
        // 保持原签名行为，走默认（官方）context
        setInfo(g2pId, QString(), QVersionNumber());
    }

    void G2pInfoWidget::setInfo(const QString &g2pId, const QString &context,
                                const QVersionNumber &contextVersion) const {
        // 用解析出的 (context, version) 取代硬编码 ""，使声库自定义 G2P
        // 在调试/详情页能够正确加载。
        // 当 context 为空且 version 为 null 时，与改造前 3 参数版本 task("g2p","",g2pId)
        // 完全等价（走官方默认 context），避免依赖 stdc::VersionNumber(0,0,0,0).isEmpty()
        // 的具体行为差异
        const auto contextStd = context.toStdString();
        srt::core::Expected<srt::core::NO<srt::g2p::Task>> g2pFactory;
        if (context.isEmpty() && contextVersion.isNull()) {
            g2pFactory = srt::g2p::Manager::instance()->task("g2p", contextStd, g2pId.toStdString());
        } else {
            const auto versionStd = VersionUtils::qt_to_stdc(contextVersion);
            g2pFactory = srt::g2p::Manager::instance()->task(
                "g2p", contextStd, versionStd, g2pId.toStdString());
        }

        if (!g2pFactory) {
            // 路由加载失败时给出可诊断的 UI 提示，避免静默无反馈
            // 补充分类日志，便于无控制台时排障
            const auto contextDisplay = context.isEmpty() ? tr("(official)") : context;
            const auto versionDisplay = contextVersion.isNull() ? tr("N/A") : contextVersion.toString();
            qCWarning(logLangSetting).nospace()
                << "G2P config load failed g2pId='" << g2pId << "' context='" << context
                << "' version=" << contextVersion.toString()
                << " reason='" << QString::fromStdString(g2pFactory.error().message()) << "'";
            m_label->setText(tr("G2P Config (Load Failed)"));
            m_descriptionLabel->setText(
                tr("Failed to load g2p '%1' in context '%2' (version %3). "
                   "Check voicebank G2P package installation.")
                    .arg(g2pId, contextDisplay, versionDisplay));
            return;
        }

        // 成功加载时重置可能的失败态 UI（避免前一次失败的文案残留）
        // 已知限制：LangCore::Task 接口暂未提供 description/displayName/author，因此详细信息
        // （语言/作者/描述、langConfig/g2pConfig 子控件）无法展示。
        // 详见 docs/g2p-architecture/README.md §5 待办任务。
        m_label->setText(tr("G2P Config"));
        m_descriptionLabel->setText(tr("G2P '%1' loaded.").arg(g2pId));
    }
} // LangMgr