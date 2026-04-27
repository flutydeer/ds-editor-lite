#include "SplitterConfigTab.h"

#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <re2/re2.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"

#include "RuleListItemWidget.h"
#include "RuleListPanel.h"
#include "RuleListWidget.h"
#include "SplitterDetailPanel.h"

namespace FillLyric
{
    // ── Helper: validate RE2 regex ──────────────────────────────────────────

    static bool validateRegex(const QString &pattern, QString &errorMsg) {
        RE2::Options opts;
        opts.set_encoding(RE2::Options::EncodingUTF8);
        opts.set_log_errors(false);
        RE2 re(pattern.toStdString(), opts);
        if (!re.ok()) {
            errorMsg = QString::fromStdString(re.error());
            return false;
        }
        return true;
    }

    SplitterConfigTab::SplitterConfigTab(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("SplitterConfigTab");

        auto *outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(8, 8, 8, 8);

        auto *splitter = new QSplitter(Qt::Horizontal);
        splitter->setChildrenCollapsible(false);

        m_listPanel = new RuleListPanel;
        splitter->addWidget(m_listPanel);

        m_detailPanel = new SplitterDetailPanel;
        splitter->addWidget(m_detailPanel);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 2);

        outerLayout->addWidget(splitter, 1);

        // Info + Apply row
        auto *bottomLayout = new QHBoxLayout;
        auto *infoLabel = new QLabel(tr(
            "Splitter rules only affect Auto split mode. "
            "Regexes are applied in list order from top to bottom."));
        infoLabel->setObjectName("ruleInfoLabel");
        infoLabel->setWordWrap(true);
        bottomLayout->addWidget(infoLabel, 1);

        m_applyBtn = new QPushButton(tr("Apply"));
        bottomLayout->addWidget(m_applyBtn);

        auto *testJumpBtn = new QPushButton(tr("Test >>"));
        testJumpBtn->setToolTip(tr("Jump to Test tab to preview split/tag results"));
        bottomLayout->addWidget(testJumpBtn);

        outerLayout->addLayout(bottomLayout);

        // Connections
        connect(m_listPanel, &RuleListPanel::addRequested, this, &SplitterConfigTab::onAddRule);
        connect(m_listPanel, &RuleListPanel::removeRequested, this, &SplitterConfigTab::onRemoveRule);
        connect(m_listPanel->listWidget(), &RuleListWidget::orderChanged, this, &SplitterConfigTab::onOrderChanged);
        connect(m_listPanel->listWidget(), &QListWidget::currentRowChanged, this, &SplitterConfigTab::onSelectionChanged);
        connect(m_applyBtn, &QPushButton::clicked, this, &SplitterConfigTab::applyConfig);
        connect(testJumpBtn, &QPushButton::clicked, this, &SplitterConfigTab::jumpToTestRequested);
    }

    void SplitterConfigTab::loadFromOption(FillLyricOption *opt) {
        m_rules.clear();
        m_currentIndex = -1;

        // Get all rules from engine
        const auto infos = TextSplitter::ruleInfoList();

        // Build unified list: if we have a saved order, use it; otherwise use engine order
        if (!opt->splitterOrder.isEmpty()) {
            // Add rules in saved order
            for (const auto &name : opt->splitterOrder) {
                RuleItem item;
                item.name = name;

                // Find in engine infos
                bool found = false;
                for (const auto &info : infos) {
                    if (info.name == name) {
                        item.builtin = info.builtin;
                        item.enabled = info.enabled;
                        item.builtinInfo = info;
                        found = true;
                        // For non-builtin rules, also restore customRule from saved options
                        if (!info.builtin) {
                            for (const auto &cr : opt->customSplitterRules) {
                                if (cr.name == name) {
                                    item.customRule = cr;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
                if (!found) {
                    // Check custom rules not yet in engine
                    for (const auto &cr : opt->customSplitterRules) {
                        if (cr.name == name) {
                            item.builtin = false;
                            item.enabled = cr.enabled;
                            item.customRule = cr;
                            found = true;
                            break;
                        }
                    }
                }
                if (found)
                    m_rules.append(item);
            }

            // Append any rules not in saved order (new builtins)
            for (const auto &info : infos) {
                bool exists = false;
                for (const auto &r : m_rules) {
                    if (r.name == info.name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    RuleItem item;
                    item.name = info.name;
                    item.builtin = info.builtin;
                    item.enabled = info.enabled;
                    item.builtinInfo = info;
                    m_rules.append(item);
                }
            }
        } else {
            // Default: engine order for builtins, then custom rules
            for (const auto &info : infos) {
                if (!info.builtin)
                    continue;
                RuleItem item;
                item.name = info.name;
                item.builtin = true;
                item.enabled = opt->builtinSplitterEnabled.value(info.name, true);
                item.builtinInfo = info;
                m_rules.append(item);
            }
            for (const auto &cr : opt->customSplitterRules) {
                RuleItem item;
                item.name = cr.name;
                item.builtin = false;
                item.enabled = cr.enabled;
                item.customRule = cr;
                m_rules.append(item);
            }
        }

        rebuildItemWidgets();
        m_detailPanel->showPlaceholder();
    }

    void SplitterConfigTab::rebuildItemWidgets() {
        auto *list = m_listPanel->listWidget();
        list->blockSignals(true);
        list->clear();

        for (int i = 0; i < m_rules.size(); i++) {
            const auto &rule = m_rules[i];
            auto *item = new QListWidgetItem;
            item->setSizeHint(QSize(0, 32));
            item->setData(Qt::UserRole, i); // store rule index for drag-drop tracking
            list->addItem(item);

            auto *widget = new RuleListItemWidget(rule.name, rule.builtin, rule.enabled);
            list->setItemWidget(item, widget);

            connect(widget, &RuleListItemWidget::enabledChanged, this, [this, i](bool enabled) {
                if (i >= 0 && i < m_rules.size())
                    m_rules[i].enabled = enabled;
            });
        }

        list->blockSignals(false);
    }

    void SplitterConfigTab::onSelectionChanged(int currentRow) {
        // Save previous edit
        saveCurrentDetail();

        m_currentIndex = currentRow;

        if (currentRow < 0 || currentRow >= m_rules.size()) {
            m_detailPanel->showPlaceholder();
            m_listPanel->setRemoveEnabled(false);
            return;
        }

        const auto &rule = m_rules[currentRow];
        if (rule.builtin) {
            m_detailPanel->showBuiltinRule(rule.builtinInfo);
            m_listPanel->setRemoveEnabled(false);
        } else {
            m_detailPanel->showCustomRule(rule.customRule);
            m_listPanel->setRemoveEnabled(true);
        }
    }

    void SplitterConfigTab::saveCurrentDetail() {
        if (m_currentIndex < 0 || m_currentIndex >= m_rules.size())
            return;
        auto &rule = m_rules[m_currentIndex];
        if (!rule.builtin) {
            auto collected = m_detailPanel->collectCustomRule();
            collected.enabled = rule.enabled;
            rule.customRule = collected;
            rule.name = collected.name;
        }
    }

    void SplitterConfigTab::onAddRule() {
        saveCurrentDetail();

        RuleItem item;
        item.name = tr("new-rule");
        item.builtin = false;
        item.enabled = true;
        item.customRule.name = item.name;
        item.customRule.enabled = true;
        m_rules.append(item);

        rebuildItemWidgets();

        auto *list = m_listPanel->listWidget();
        list->setCurrentRow(m_rules.size() - 1);
    }

    void SplitterConfigTab::onRemoveRule() {
        if (m_currentIndex < 0 || m_currentIndex >= m_rules.size())
            return;
        if (m_rules[m_currentIndex].builtin)
            return;

        m_rules.removeAt(m_currentIndex);
        m_currentIndex = -1;
        rebuildItemWidgets();
        m_detailPanel->showPlaceholder();
        m_listPanel->setRemoveEnabled(false);
    }

    void SplitterConfigTab::onOrderChanged() {
        // After drag-drop, read back the order from QListWidgetItem::data(UserRole)
        // which stores the original index in m_rules.
        auto *list = m_listPanel->listWidget();
        QList<RuleItem> reordered;
        reordered.reserve(m_rules.size());

        for (int i = 0; i < list->count(); i++) {
            const int origIdx = list->item(i)->data(Qt::UserRole).toInt();
            if (origIdx >= 0 && origIdx < m_rules.size())
                reordered.append(m_rules[origIdx]);
        }

        m_rules = reordered;
        m_currentIndex = -1;

        // Rebuild widgets to fix up indices and restore lost item widgets
        rebuildItemWidgets();
        m_detailPanel->showPlaceholder();
    }

    void SplitterConfigTab::applyConfig() {
        saveCurrentDetail();

        // Validate all custom rules
        for (int i = 0; i < m_rules.size(); i++) {
            if (m_rules[i].builtin)
                continue;
            const auto &cr = m_rules[i].customRule;
            if (cr.name.trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Error"),
                                     tr("Custom rule #%1 has an empty name.").arg(i + 1));
                m_listPanel->listWidget()->setCurrentRow(i);
                return;
            }
            for (const auto &regex : cr.regexes) {
                QString err;
                if (!validateRegex(regex, err)) {
                    QMessageBox::warning(this, tr("Invalid Regex"),
                                         tr("Rule \"%1\": regex error in \"%2\": %3")
                                             .arg(cr.name, regex, err));
                    m_listPanel->listWidget()->setCurrentRow(i);
                    return;
                }
            }
        }

        auto *opt = appOptions->fillLyric();

        // Collect enabled states and custom rules
        opt->builtinSplitterEnabled.clear();
        opt->customSplitterRules.clear();
        QStringList order;

        for (const auto &rule : m_rules) {
            order.append(rule.name);
            if (rule.builtin) {
                opt->builtinSplitterEnabled[rule.name] = rule.enabled;
            } else {
                auto cr = rule.customRule;
                cr.enabled = rule.enabled;
                opt->customSplitterRules.append(cr);
            }
        }
        opt->splitterOrder = order;

        // Save to disk
        appOptions->saveAndNotify(AppOptionsGlobal::FillLyric);

        // Apply to engine
        TextSplitter::setBuiltinEnabled(opt->builtinSplitterEnabled);
        TextSplitter::setCustomRules(opt->customSplitterRules);
        TextSplitter::setRuleOrder(opt->splitterOrder);

        emit configChanged();
    }

} // namespace FillLyric
