#include "TaggerConfigTab.h"

#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <re2/re2.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include "RuleListItemWidget.h"
#include "RuleListPanel.h"
#include "RuleListWidget.h"
#include "TaggerDetailPanel.h"

namespace FillLyric
{
    static bool validateTaggerRegex(const QString &pattern, QString &errorMsg) {
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

    TaggerConfigTab::TaggerConfigTab(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("TaggerConfigTab");

        auto *outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(8, 8, 8, 8);

        auto *splitter = new QSplitter(Qt::Horizontal);
        splitter->setChildrenCollapsible(false);

        m_listPanel = new RuleListPanel;
        splitter->addWidget(m_listPanel);

        m_detailPanel = new TaggerDetailPanel;
        splitter->addWidget(m_detailPanel);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 2);

        outerLayout->addWidget(splitter, 1);

        // Info + Apply row
        auto *bottomLayout = new QHBoxLayout;
        auto *infoLabel = new QLabel(tr(
            "Tagger rules affect all split modes. "
            "Rules only match tokens with language=\"unknown\"; first match wins."));
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
        connect(m_listPanel, &RuleListPanel::addRequested, this, &TaggerConfigTab::onAddRule);
        connect(m_listPanel, &RuleListPanel::removeRequested, this, &TaggerConfigTab::onRemoveRule);
        connect(m_listPanel->listWidget(), &RuleListWidget::orderChanged, this, &TaggerConfigTab::onOrderChanged);
        connect(m_listPanel->listWidget(), &QListWidget::currentRowChanged, this, &TaggerConfigTab::onSelectionChanged);
        connect(m_applyBtn, &QPushButton::clicked, this, &TaggerConfigTab::applyConfig);
        connect(testJumpBtn, &QPushButton::clicked, this, &TaggerConfigTab::jumpToTestRequested);
    }

    void TaggerConfigTab::loadFromOption(FillLyricOption *opt) {
        m_rules.clear();
        m_currentIndex = -1;

        const auto infos = TextTagger::ruleInfoList();
        m_knownLanguages = TextTagger::builtinLanguages();

        if (!opt->taggerOrder.isEmpty()) {
            for (const auto &name : opt->taggerOrder) {
                RuleItem item;
                item.name = name;

                bool found = false;
                for (const auto &info : infos) {
                    if (info.language == name && info.builtin) {
                        item.builtin = true;
                        item.enabled = info.enabled;
                        item.builtinInfo = info;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (const auto &cr : opt->customTaggerRules) {
                        if (cr.language == name) {
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

            // Append new builtins not in saved order
            for (const auto &info : infos) {
                if (!info.builtin)
                    continue;
                bool exists = false;
                for (const auto &r : m_rules) {
                    if (r.name == info.language) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    RuleItem item;
                    item.name = info.language;
                    item.builtin = true;
                    item.enabled = info.enabled;
                    item.builtinInfo = info;
                    m_rules.append(item);
                }
            }
        } else {
            for (const auto &info : infos) {
                if (!info.builtin)
                    continue;
                RuleItem item;
                item.name = info.language;
                item.builtin = true;
                item.enabled = opt->builtinTaggerEnabled.value(info.language, true);
                item.builtinInfo = info;
                m_rules.append(item);
            }
            for (const auto &cr : opt->customTaggerRules) {
                RuleItem item;
                item.name = cr.language;
                item.builtin = false;
                item.enabled = cr.enabled;
                item.customRule = cr;
                m_rules.append(item);
            }
        }

        rebuildItemWidgets();
        m_detailPanel->showPlaceholder();
    }

    void TaggerConfigTab::rebuildItemWidgets() {
        auto *list = m_listPanel->listWidget();
        list->blockSignals(true);
        list->clear();

        for (int i = 0; i < m_rules.size(); i++) {
            const auto &rule = m_rules[i];
            auto *item = new QListWidgetItem;
            item->setSizeHint(QSize(0, 32));
            item->setData(Qt::UserRole, i);
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

    void TaggerConfigTab::onSelectionChanged(int currentRow) {
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
            m_detailPanel->showCustomRule(rule.customRule, m_knownLanguages);
            m_listPanel->setRemoveEnabled(true);
        }
    }

    void TaggerConfigTab::saveCurrentDetail() {
        if (m_currentIndex < 0 || m_currentIndex >= m_rules.size())
            return;
        auto &rule = m_rules[m_currentIndex];
        if (!rule.builtin) {
            auto collected = m_detailPanel->collectCustomRule();
            collected.enabled = rule.enabled;
            rule.customRule = collected;
            rule.name = collected.language;
        }
    }

    void TaggerConfigTab::onAddRule() {
        saveCurrentDetail();

        RuleItem item;
        item.name = "cmn";
        item.builtin = false;
        item.enabled = true;
        item.customRule.language = "cmn";
        item.customRule.enabled = true;
        m_rules.append(item);

        rebuildItemWidgets();

        auto *list = m_listPanel->listWidget();
        list->setCurrentRow(m_rules.size() - 1);
    }

    void TaggerConfigTab::onRemoveRule() {
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

    void TaggerConfigTab::onOrderChanged() {
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

        rebuildItemWidgets();
        m_detailPanel->showPlaceholder();
    }

    void TaggerConfigTab::applyConfig() {
        saveCurrentDetail();

        // Validate custom rules
        for (int i = 0; i < m_rules.size(); i++) {
            if (m_rules[i].builtin)
                continue;
            const auto &cr = m_rules[i].customRule;
            if (cr.language.trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Error"),
                                     tr("Custom rule #%1 has an empty language.").arg(i + 1));
                m_listPanel->listWidget()->setCurrentRow(i);
                return;
            }
            for (const auto &entry : cr.entries) {
                if (entry.type == "regex") {
                    for (const auto &v : entry.value) {
                        QString err;
                        if (!validateTaggerRegex(v, err)) {
                            QMessageBox::warning(this, tr("Invalid Regex"),
                                                 tr("Rule \"%1\": regex error in \"%2\": %3")
                                                     .arg(cr.language, v, err));
                            m_listPanel->listWidget()->setCurrentRow(i);
                            return;
                        }
                    }
                }
            }
        }

        auto *opt = appOptions->fillLyric();

        opt->builtinTaggerEnabled.clear();
        opt->customTaggerRules.clear();
        QStringList order;

        for (const auto &rule : m_rules) {
            order.append(rule.name);
            if (rule.builtin) {
                opt->builtinTaggerEnabled[rule.name] = rule.enabled;
            } else {
                auto cr = rule.customRule;
                cr.enabled = rule.enabled;
                opt->customTaggerRules.append(cr);
            }
        }
        opt->taggerOrder = order;

        appOptions->saveAndNotify(AppOptionsGlobal::FillLyric);

        TextTagger::setBuiltinEnabled(opt->builtinTaggerEnabled);
        TextTagger::setCustomRules(opt->customTaggerRules);
        TextTagger::setRuleOrder(opt->taggerOrder);

        emit configChanged();
    }

} // namespace FillLyric
