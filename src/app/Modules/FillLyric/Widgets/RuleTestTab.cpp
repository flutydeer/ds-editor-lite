#include "RuleTestTab.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

namespace FillLyric
{
    RuleTestTab::RuleTestTab(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("RuleTestTab");

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(8);

        // ── Input area ──────────────────────────────────────────────────────
        auto *inputGroup = new QGroupBox(tr("Input Text"));
        auto *inputLayout = new QVBoxLayout(inputGroup);
        inputLayout->setContentsMargins(8, 8, 8, 8);

        m_inputEdit = new QPlainTextEdit;
        m_inputEdit->setObjectName("testInputEdit");
        m_inputEdit->setPlaceholderText(
            tr("Enter text here, e.g.: \"小酒窝长睫毛是你最美的记号\""));
        m_inputEdit->setMinimumHeight(48);
        m_inputEdit->setMaximumHeight(80);
        inputLayout->addWidget(m_inputEdit);

        auto *inputBtnRow = new QHBoxLayout;
        m_errorLabel = new QLabel;
        m_errorLabel->setObjectName("testErrorLabel");
        m_errorLabel->setWordWrap(true);
        inputBtnRow->addWidget(m_errorLabel, 1);

        m_runBtn = new QPushButton(tr("Run Test"));
        inputBtnRow->addWidget(m_runBtn);
        inputLayout->addLayout(inputBtnRow);

        mainLayout->addWidget(inputGroup);

        // ── Step 1: Split result ────────────────────────────────────────────
        auto *splitGroup = new QGroupBox(tr("Step 1 - Split Result"));
        auto *splitLayout = new QVBoxLayout(splitGroup);
        splitLayout->setContentsMargins(8, 8, 8, 8);

        auto *splitHeaderRow = new QHBoxLayout;
        auto *splitDesc = new QLabel(
            tr("Text is split into tokens by splitter rules (applied in order)."));
        splitDesc->setObjectName("ruleInfoLabel");
        splitDesc->setWordWrap(true);
        splitHeaderRow->addWidget(splitDesc, 1);

        auto *goSplitterBtn = new QPushButton(tr("Edit Splitter >>"));
        goSplitterBtn->setToolTip(tr("Jump to Splitter config tab"));
        splitHeaderRow->addWidget(goSplitterBtn);
        splitLayout->addLayout(splitHeaderRow);

        m_splitOutput = new QPlainTextEdit;
        m_splitOutput->setObjectName("testOutputEdit");
        m_splitOutput->setReadOnly(true);
        m_splitOutput->setMinimumHeight(48);
        splitLayout->addWidget(m_splitOutput, 1);

        mainLayout->addWidget(splitGroup, 1);

        // ── Step 2: Tag result ──────────────────────────────────────────────
        auto *tagGroup = new QGroupBox(tr("Step 2 - Tag Result"));
        auto *tagLayout = new QVBoxLayout(tagGroup);
        tagLayout->setContentsMargins(8, 8, 8, 8);

        auto *tagHeaderRow = new QHBoxLayout;
        auto *tagDesc = new QLabel(
            tr("Each token from step 1 is tagged with language/tag by tagger rules (first match wins)."));
        tagDesc->setObjectName("ruleInfoLabel");
        tagDesc->setWordWrap(true);
        tagHeaderRow->addWidget(tagDesc, 1);

        auto *goTaggerBtn = new QPushButton(tr("Edit Tagger >>"));
        goTaggerBtn->setToolTip(tr("Jump to Tagger config tab"));
        tagHeaderRow->addWidget(goTaggerBtn);
        tagLayout->addLayout(tagHeaderRow);

        m_tagOutput = new QPlainTextEdit;
        m_tagOutput->setObjectName("testOutputEdit");
        m_tagOutput->setReadOnly(true);
        m_tagOutput->setMinimumHeight(48);
        tagLayout->addWidget(m_tagOutput, 1);

        mainLayout->addWidget(tagGroup, 1);

        // Connections
        connect(m_runBtn, &QPushButton::clicked, this, &RuleTestTab::runTest);
        connect(goSplitterBtn, &QPushButton::clicked, this, &RuleTestTab::jumpToSplitterRequested);
        connect(goTaggerBtn, &QPushButton::clicked, this, &RuleTestTab::jumpToTaggerRequested);
    }

    void RuleTestTab::runTest() {
        const auto input = m_inputEdit->toPlainText().trimmed();
        if (input.isEmpty()) {
            m_splitOutput->clear();
            m_tagOutput->clear();
            m_errorLabel->clear();
            return;
        }

        m_errorLabel->clear();
        m_errorLabel->setStyleSheet("");

        // ── Step 1: Split ───────────────────────────────────────────────────
        const auto splitResult = TextSplitter::split(input.toStdString());

        QStringList splitTokens;
        splitTokens.reserve(static_cast<int>(splitResult.size()));
        for (const auto &token : splitResult)
            splitTokens.append(QString::fromStdString(token));

        m_splitOutput->setPlainText(
            tr("Token count: %1\n").arg(splitResult.size()) +
            splitTokens.join(QStringLiteral(" | ")));

        // ── Step 2: Tag (uses split result as input) ────────────────────────
        const auto tagResult = TextTagger::tag(splitResult);

        QStringList tagLines;
        tagLines.reserve(static_cast<int>(tagResult.size()));
        for (const auto &r : tagResult) {
            tagLines.append(QStringLiteral("%1  [lang=%2, tag=%3%4]")
                                .arg(QString::fromStdString(r.lyric),
                                     QString::fromStdString(r.language),
                                     QString::fromStdString(r.tag),
                                     r.discard ? QStringLiteral(", discard") : QString()));
        }

        m_tagOutput->setPlainText(tagLines.join('\n'));
    }

} // namespace FillLyric
