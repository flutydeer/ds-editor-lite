#include "TaggerDetailPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace FillLyric
{
    TaggerDetailPanel::TaggerDetailPanel(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("TaggerDetailPanel");

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

        bLayout->addWidget(new QLabel(tr("Entries:")));
        m_builtinEntriesView = new QPlainTextEdit;
        m_builtinEntriesView->setReadOnly(true);
        bLayout->addWidget(m_builtinEntriesView, 1);

        auto *builtinInfo = new QLabel(tr("Built-in rules cannot be edited."));
        builtinInfo->setObjectName("ruleInfoLabel");
        builtinInfo->setWordWrap(true);
        bLayout->addWidget(builtinInfo);

        bLayout->addStretch();
        m_stack->addWidget(builtinPage);

        // Page 2: custom editable
        auto *customPage = new QWidget;
        auto *cLayout = new QVBoxLayout(customPage);
        cLayout->setContentsMargins(12, 12, 12, 12);

        auto *langRow = new QHBoxLayout;
        langRow->addWidget(new QLabel(tr("Language:")));
        m_langCombo = new QComboBox;
        m_langCombo->setEditable(true);
        langRow->addWidget(m_langCombo);
        cLayout->addLayout(langRow);

        auto *entriesHeader = new QHBoxLayout;
        entriesHeader->addWidget(new QLabel(tr("Entries:")));
        entriesHeader->addStretch();
        m_addEntryBtn = new QPushButton(tr("+ Add Entry"));
        entriesHeader->addWidget(m_addEntryBtn);
        cLayout->addLayout(entriesHeader);

        m_entriesLayout = new QVBoxLayout;
        cLayout->addLayout(m_entriesLayout);

        auto *infoLabel = new QLabel(tr(
            "Language must be a G2p-registered code (e.g. cmn, eng, jpn, yue).\n"
            "Regex values are merged with | for FullMatch. Array values are exact match."));
        infoLabel->setObjectName("ruleInfoLabel");
        infoLabel->setWordWrap(true);
        cLayout->addWidget(infoLabel);

        cLayout->addStretch();
        m_stack->addWidget(customPage);

        layout->addWidget(m_stack);

        connect(m_addEntryBtn, &QPushButton::clicked, this, [this]() {
            CustomTaggerEntry empty;
            empty.type = "regex";
            addCustomEntryUI(empty);
        });

        showPlaceholder();
    }

    void TaggerDetailPanel::showPlaceholder() {
        m_stack->setCurrentIndex(0);
    }

    void TaggerDetailPanel::showBuiltinRule(const TaggerRuleInfo &info) {
        m_builtinNameLabel->setText(info.language + "  (built-in)");

        QString text;
        for (int i = 0; i < info.entries.size(); i++) {
            const auto &e = info.entries[i];
            text += QStringLiteral("#%1  type=%2  tag=%3").arg(i + 1).arg(e.type, e.tag);
            if (e.discard)
                text += QStringLiteral("  (discard)");
            text += '\n';
            for (const auto &v : e.values)
                text += QStringLiteral("    %1\n").arg(v);
        }
        m_builtinEntriesView->setPlainText(text);
        m_stack->setCurrentIndex(1);
    }

    void TaggerDetailPanel::showCustomRule(const CustomTaggerRule &rule,
                                            const QStringList &knownLanguages) {
        m_knownLanguages = knownLanguages;
        m_langCombo->clear();
        for (const auto &lang : knownLanguages)
            m_langCombo->addItem(lang);
        m_langCombo->setCurrentText(rule.language);

        clearCustomEntries();
        for (const auto &entry : rule.entries)
            addCustomEntryUI(entry);

        m_stack->setCurrentIndex(2);
    }

    CustomTaggerRule TaggerDetailPanel::collectCustomRule() const {
        CustomTaggerRule rule;
        rule.language = m_langCombo->currentText().trimmed();
        rule.enabled = true; // managed by list checkbox

        for (const auto &er : m_entryRows) {
            CustomTaggerEntry entry;
            entry.type = er.typeCombo->currentText();
            entry.tag = er.tagEdit->text().trimmed();
            entry.discard = er.discardCheck->isChecked();
            const auto lines = er.valuesEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
            for (const auto &line : lines) {
                const auto trimmed = line.trimmed();
                if (!trimmed.isEmpty())
                    entry.value.append(trimmed);
            }
            if (!entry.tag.isEmpty() && !entry.value.isEmpty())
                rule.entries.append(entry);
        }
        return rule;
    }

    void TaggerDetailPanel::setKnownLanguages(const QStringList &languages) {
        m_knownLanguages = languages;
    }

    void TaggerDetailPanel::addCustomEntryUI(const CustomTaggerEntry &entry) {
        auto *entryWidget = new QWidget;
        auto *eLayout = new QVBoxLayout(entryWidget);
        eLayout->setContentsMargins(4, 4, 4, 4);

        auto *row1 = new QHBoxLayout;
        auto *typeCombo = new QComboBox;
        typeCombo->addItems({"regex", "array"});
        typeCombo->setCurrentText(entry.type);
        row1->addWidget(new QLabel(tr("Type:")));
        row1->addWidget(typeCombo);

        auto *tagEdit = new QLineEdit(entry.tag);
        row1->addWidget(new QLabel(tr("Tag:")));
        row1->addWidget(tagEdit);

        auto *discardCheck = new QCheckBox(tr("Discard"));
        discardCheck->setChecked(entry.discard);
        row1->addWidget(discardCheck);

        auto *removeBtn = new QPushButton(tr("Remove"));
        row1->addWidget(removeBtn);
        eLayout->addLayout(row1);

        auto *valuesEdit = new QPlainTextEdit;
        valuesEdit->setPlainText(entry.value.join('\n'));
        valuesEdit->setMaximumHeight(60);
        eLayout->addWidget(valuesEdit);

        m_entriesLayout->addWidget(entryWidget);

        EntryRowUI er{entryWidget, typeCombo, tagEdit, valuesEdit, discardCheck};
        m_entryRows.append(er);

        connect(removeBtn, &QPushButton::clicked, entryWidget, [this, entryWidget]() {
            for (int i = 0; i < m_entryRows.size(); i++) {
                if (m_entryRows[i].widget == entryWidget) {
                    m_entryRows.removeAt(i);
                    break;
                }
            }
            entryWidget->deleteLater();
        });
    }

    void TaggerDetailPanel::clearCustomEntries() {
        for (auto &er : m_entryRows) {
            er.widget->deleteLater();
        }
        m_entryRows.clear();
    }

} // namespace FillLyric
