#include "RuleListItemWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>

#include <lite/GUI/Utils/IconUtils.h>

namespace FillLyric {
    RuleListItemWidget::RuleListItemWidget(const QString &name, bool builtin, bool enabled,
                                           QWidget *parent)
        : QWidget(parent), m_builtin(builtin) {

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(4);

        const QSize iconSize(16, 16);
        const auto iconPalette = IconUtils::defaultActionPalette();

        m_handleLabel = new QLabel;
        m_handleLabel->setObjectName("dragHandle");
        m_handleLabel->setPixmap(
            IconUtils::createTintedSvgIcon(
                QStringLiteral(":/svg/icons/re_order_dots_vertical_16_regular.svg"), iconSize,
                iconPalette)
                .pixmap(iconSize));
        m_handleLabel->setFixedSize(iconSize);
        m_handleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_handleLabel);

        m_checkbox = new QCheckBox;
        m_checkbox->setChecked(enabled);
        layout->addWidget(m_checkbox);

        m_nameLabel = new QLabel(name);
        m_nameLabel->setObjectName("ruleNameLabel");
        layout->addWidget(m_nameLabel, 1);

        if (builtin) {
            m_builtinLabel = new QLabel;
            m_builtinLabel->setObjectName("builtinIcon");
            m_builtinLabel->setToolTip(tr("Built-in rule (read-only)"));
            m_builtinLabel->setPixmap(
                IconUtils::createTintedSvgIcon(
                    QStringLiteral(":/svg/icons/lock_closed_16_regular.svg"), iconSize, iconPalette)
                    .pixmap(iconSize));
            m_builtinLabel->setFixedWidth(20);
            m_builtinLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(m_builtinLabel);
        }

        connect(m_checkbox, &QCheckBox::toggled, this, &RuleListItemWidget::enabledChanged);
    }

    bool RuleListItemWidget::isRuleEnabled() const {
        return m_checkbox->isChecked();
    }

    void RuleListItemWidget::setRuleEnabled(bool enabled) {
        m_checkbox->setChecked(enabled);
    }

    QString RuleListItemWidget::name() const {
        return m_nameLabel->text();
    }

    bool RuleListItemWidget::isBuiltin() const {
        return m_builtin;
    }

} // namespace FillLyric
