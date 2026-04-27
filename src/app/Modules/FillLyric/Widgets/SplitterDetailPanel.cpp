#include "SplitterDetailPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace FillLyric
{
    SplitterDetailPanel::SplitterDetailPanel(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("SplitterDetailPanel");

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_stack = new QStackedWidget;

        // Page 0: placeholder
        auto *placeholderPage = new QWidget;
        auto *phLayout = new QVBoxLayout(placeholderPage);
        m_placeholderLabel = new QLabel(tr("Select a rule from the list to view or edit details."));
        m_placeholderLabel->setObjectName("ruleInfoLabel");
        m_placeholderLabel->setAlignment(Qt::AlignCenter);
        m_placeholderLabel->setWordWrap(true);
        phLayout->addWidget(m_placeholderLabel);
        m_stack->addWidget(placeholderPage);

        // Page 1: builtin readonly
        auto *builtinPage = new QWidget;
        auto *bLayout = new QVBoxLayout(builtinPage);
        bLayout->setContentsMargins(12, 12, 12, 12);

        m_builtinNameLabel = new QLabel;
        m_builtinNameLabel->setObjectName("ruleDetailTitle");
        bLayout->addWidget(m_builtinNameLabel);

        bLayout->addWidget(new QLabel(tr("Regexes:")));
        m_builtinRegexView = new QPlainTextEdit;
        m_builtinRegexView->setReadOnly(true);
        bLayout->addWidget(m_builtinRegexView, 1);

        auto *builtinInfo = new QLabel(tr(
            "Built-in rules cannot be edited.\n"
            "Create a custom rule and reorder via drag-and-drop if needed."));
        builtinInfo->setObjectName("ruleInfoLabel");
        builtinInfo->setWordWrap(true);
        bLayout->addWidget(builtinInfo);

        bLayout->addStretch();
        m_stack->addWidget(builtinPage);

        // Page 2: custom editable
        auto *customPage = new QWidget;
        auto *cLayout = new QVBoxLayout(customPage);
        cLayout->setContentsMargins(12, 12, 12, 12);

        auto *nameRow = new QHBoxLayout;
        nameRow->addWidget(new QLabel(tr("Name:")));
        m_nameEdit = new QLineEdit;
        nameRow->addWidget(m_nameEdit);
        cLayout->addLayout(nameRow);

        cLayout->addWidget(new QLabel(tr("Regexes (one RE2 pattern per line):")));
        m_regexEdit = new QPlainTextEdit;
        m_regexEdit->setMinimumHeight(100);
        cLayout->addWidget(m_regexEdit, 1);

        m_infoLabel = new QLabel(tr(
            "Each regex must contain one capture group ().\n"
            "Matched and unmatched parts are kept as separate tokens.\n"
            "Uses RE2 syntax (no backreferences or lookahead)."));
        m_infoLabel->setObjectName("ruleInfoLabel");
        m_infoLabel->setWordWrap(true);
        cLayout->addWidget(m_infoLabel);

        cLayout->addStretch();
        m_stack->addWidget(customPage);

        layout->addWidget(m_stack);
        showPlaceholder();
    }

    void SplitterDetailPanel::showPlaceholder() {
        m_stack->setCurrentIndex(0);
    }

    void SplitterDetailPanel::showBuiltinRule(const SplitterRuleInfo &info) {
        m_builtinNameLabel->setText(info.name + "  (built-in)");
        m_builtinRegexView->setPlainText(info.regexes.join('\n'));
        m_stack->setCurrentIndex(1);
    }

    void SplitterDetailPanel::showCustomRule(const CustomSplitterRule &rule) {
        m_nameEdit->setText(rule.name);
        m_regexEdit->setPlainText(rule.regexes.join('\n'));
        m_stack->setCurrentIndex(2);
    }

    CustomSplitterRule SplitterDetailPanel::collectCustomRule() const {
        CustomSplitterRule rule;
        rule.name = m_nameEdit->text().trimmed();
        rule.enabled = true; // enabled state is managed by the list checkbox
        rule.order = 0;
        const auto lines = m_regexEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            const auto trimmed = line.trimmed();
            if (!trimmed.isEmpty())
                rule.regexes.append(trimmed);
        }
        return rule;
    }

} // namespace FillLyric
