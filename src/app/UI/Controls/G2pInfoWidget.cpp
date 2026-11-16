#include "UI/Controls/G2pInfoWidget.h"

#include <lite/Support/VersionUtils.h>

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
        // An empty selection is not a load failure.
        if (g2pId.isEmpty()) {
            m_label->setText(tr("G2P Config"));
            m_descriptionLabel->setText(tr("(no G2P preset selected)"));
            return;
        }
        setInfo(g2pId, QString(), QVersionNumber());
    }

    void G2pInfoWidget::setInfo(const QString &g2pId, const QString &context,
                                const QVersionNumber &contextVersion) const {
        // The unversioned overload selects the official context without converting a null version.
        const auto contextStd = context.toStdString();
        srt::core::Expected<srt::core::NO<srt::g2p::Task>> g2pFactory;
        if (context.isEmpty() && contextVersion.isNull()) {
            g2pFactory =
                srt::g2p::Manager::instance()->task("g2p", contextStd, g2pId.toStdString());
        } else {
            const auto versionStd = VersionUtils::qt_to_stdc(contextVersion);
            g2pFactory = srt::g2p::Manager::instance()->task("g2p", contextStd, versionStd,
                                                             g2pId.toStdString());
        }

        if (!g2pFactory) {
            const auto contextDisplay = context.isEmpty() ? tr("(official)") : context;
            const auto versionDisplay =
                contextVersion.isNull() ? tr("N/A") : contextVersion.toString();
            qCWarning(logLangSetting).nospace()
                << "G2P config load failed g2pId='" << g2pId << "' context='" << context
                << "' version=" << contextVersion.toString() << " reason='"
                << QString::fromStdString(g2pFactory.error().message()) << "'";
            m_label->setText(tr("G2P Config (Load Failed)"));
            m_descriptionLabel->setText(tr("Failed to load g2p '%1' in context '%2' (version %3). "
                                           "Check voicebank G2P package installation.")
                                            .arg(g2pId, contextDisplay, versionDisplay));
            return;
        }

        // The task interface does not expose preset metadata for this view.
        m_label->setText(tr("G2P Config"));
        m_descriptionLabel->setText(tr("G2P '%1' loaded.").arg(g2pId));
    }
} // LangMgr
