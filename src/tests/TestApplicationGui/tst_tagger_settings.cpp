#include "tst_application_gui.h"
#include "../TestSupport/VoicebankFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include "Modules/FillLyric/Widgets/RuleListItemWidget.h"
#include "Modules/FillLyric/Widgets/RuleListWidget.h"
#include "Modules/FillLyric/Widgets/TaggerConfigTab.h"
#include "Modules/FillLyric/Widgets/TaggerDetailPanel.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    QPushButton *buttonWithText(QWidget &parent, const QString &text) {
        for (auto *button : parent.findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }

    QStringList currentEngineOrder() {
        QList<FillLyric::TaggerRuleIdentity> identities;
        for (const auto &rule : FillLyric::TextTagger::ruleInfoList())
            identities.append({.language = rule.language, .builtin = rule.builtin});
        return FillLyric::TaggerRuleOrder::canonicalize({}, identities);
    }

    void isolateTaggers(Automation::FillLyricSettingsDto &settings) {
        settings.customTaggerRules.clear();
        settings.taggerOrder.clear();
        for (const auto &language : FillLyric::TextTagger::builtinLanguages())
            settings.builtinTaggerEnabled.insert(language, false);
    }

    QByteArray savedConfiguration() {
        QFile file(appOptions->configPath());
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

    QJsonArray savedTaggers() {
        return QJsonDocument::fromJson(savedConfiguration())
            .object()
            .value(QStringLiteral("fillLyric"))
            .toObject()
            .value(QStringLiteral("customTaggerRules"))
            .toArray();
    }

    void openTaggerPage(LyricDialog &dialog) {
        dialog.show();
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        auto *tabs = dialog.findChild<QTabWidget *>();
        QVERIFY(tabs);
        int index = -1;
        for (int row = 0; row < tabs->count(); ++row) {
            if (tabs->tabText(row) == LyricDialog::tr("Tagger"))
                index = row;
        }
        QVERIFY(index >= 0);
        QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
                          tabs->tabBar()->tabRect(index).center());
        QCOMPARE(tabs->currentIndex(), index);
        auto *page = dialog.findChild<FillLyric::TaggerConfigTab *>();
        QVERIFY(page);
        QTRY_VERIFY(page->isVisible());
    }

    void selectRule(FillLyric::RuleListWidget &list, const int row) {
        auto *item = list.item(row);
        QVERIFY(item);
        list.scrollToItem(item);
        QCoreApplication::processEvents();
        const auto visibleRect = list.visualItemRect(item).intersected(list.viewport()->rect());
        QVERIFY(!visibleRect.isEmpty());
        QTest::mouseClick(list.viewport(), Qt::LeftButton, Qt::NoModifier, visibleRect.center());
        QCOMPARE(list.currentRow(), row);
    }

    FillLyric::RuleListItemWidget *customRow(FillLyric::RuleListWidget &list) {
        for (int row = 0; row < list.count(); ++row) {
            auto *widget =
                qobject_cast<FillLyric::RuleListItemWidget *>(list.itemWidget(list.item(row)));
            if (widget && !widget->isBuiltin())
                return widget;
        }
        return nullptr;
    }

    template <typename Editor>
    void typeText(Editor *editor, const QString &text) {
        QVERIFY(editor);
        QTRY_VERIFY(editor->isVisible());
        QWidget *target = editor;
        if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(editor))
            target = scrollArea->viewport();
        QTest::mouseClick(target, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keyClicks(editor, text);
    }

    struct EntryEditors {
        ComboBox *language = nullptr;
        QLineEdit *tag = nullptr;
        QPlainTextEdit *pattern = nullptr;
    };

    EntryEditors entryEditors(FillLyric::TaggerDetailPanel &details) {
        EntryEditors editors;
        for (auto *combo : details.findChildren<ComboBox *>()) {
            if (combo->isVisible() && combo->isEditable())
                editors.language = combo;
        }
        for (auto *edit : details.findChildren<QLineEdit *>()) {
            if (edit->isVisible() && (!editors.language || edit != editors.language->lineEdit()))
                editors.tag = edit;
        }
        for (auto *edit : details.findChildren<QPlainTextEdit *>()) {
            if (edit->isVisible() && !edit->isReadOnly())
                editors.pattern = edit;
        }
        return editors;
    }

    void editRule(FillLyric::TaggerDetailPanel &details, const QString &language,
                  const QString &tag, const QString &pattern) {
        const auto editors = entryEditors(details);
        QVERIFY(editors.language);
        typeText(editors.language->lineEdit(), language);
        if (QTest::currentTestFailed())
            return;
        typeText(editors.tag, tag);
        if (QTest::currentTestFailed())
            return;
        typeText(editors.pattern, pattern);
    }

    void verifyTag(const std::string &token, const std::string &language, const std::string &tag) {
        const auto tagged = FillLyric::TextTagger::tag({token});
        QCOMPARE(tagged.size(), size_t{1});
        QCOMPARE(tagged.front().lyric, token);
        QCOMPARE(tagged.front().language, language);
        QCOMPARE(tagged.front().tag, tag);
    }
}

void ApplicationGuiTests::taggerRuleInputsApplyPersistAndReopen() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().fillLyric;
    const auto engineOrder = currentEngineOrder();
    const auto restore = qScopeGuard([&] {
        QVERIFY(runtime.settings().updateFillLyric({}, original));
        FillLyric::TextTagger::setRuleOrder(engineOrder);
    });
    auto settings = original;
    isolateTaggers(settings);
    QVERIFY(runtime.settings().updateFillLyric({}, settings));
    const auto before = runtime.documentVersion();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    QList<Note *> notes;
    for (auto *note : singingClip->notes())
        notes.append(note);
    QString savedRuleId;
    {
        LyricDialog dialog(singingClip, notes, singingClip->singerIdentifier(),
                           {TestSupport::fixtureLanguage()});
        openTaggerPage(dialog);
        if (QTest::currentTestFailed())
            return;
        auto *page = dialog.findChild<FillLyric::TaggerConfigTab *>();
        auto *list = page->findChild<FillLyric::RuleListWidget *>();
        auto *details = page->findChild<FillLyric::TaggerDetailPanel *>();
        auto *add = buttonWithText(*page, QStringLiteral("+"));
        auto *apply = buttonWithText(*page, FillLyric::TaggerConfigTab::tr("Apply"));
        auto *remove = buttonWithText(*page, QStringLiteral("-"));
        QVERIFY(list && details && add && apply && remove);
        QVERIFY(list->count() > 0);
        selectRule(*list, 0);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!remove->isEnabled());
        bool displayedBuiltin = false;
        for (auto *editor : details->findChildren<QPlainTextEdit *>())
            displayedBuiltin |=
                editor->isVisible() && editor->isReadOnly() && !editor->toPlainText().isEmpty();
        QVERIFY(displayedBuiltin);

        QTest::mouseClick(add, Qt::LeftButton);
        auto *addEntry = buttonWithText(*details, FillLyric::TaggerDetailPanel::tr("+ Add Entry"));
        QVERIFY(addEntry && addEntry->isVisible());
        QTest::mouseClick(addEntry, Qt::LeftButton);
        editRule(*details, QStringLiteral("cmn"), QStringLiteral("syllable"),
                 QStringLiteral("^la[0-9]+$"));
        if (QTest::currentTestFailed())
            return;
        verifyTag("la42", "unknown", "unknown");
        QVERIFY(appOptions->fillLyric()->customTaggerRules.isEmpty());
        QSignalSpy applied(page, &FillLyric::TaggerConfigTab::configChanged);
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(applied.count(), 1);
        verifyTag("la42", "cmn", "syllable");
        QCOMPARE(appOptions->fillLyric()->customTaggerRules.size(), 1);
        savedRuleId = appOptions->fillLyric()->customTaggerRules.first().ruleId;
        QVERIFY(!savedRuleId.isEmpty());

        editRule(*details, QStringLiteral("eng"), QStringLiteral("word"),
                 QStringLiteral("^hello$"));
        if (QTest::currentTestFailed())
            return;
        verifyTag("la42", "cmn", "syllable");
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(applied.count(), 2);
        verifyTag("la42", "unknown", "unknown");
        verifyTag("hello", "eng", "word");
        QCOMPARE(appOptions->fillLyric()->customTaggerRules.first().ruleId, savedRuleId);
        auto *row = customRow(*list);
        QVERIFY(row);
        auto *enabled = row->findChild<QCheckBox *>();
        QVERIFY(enabled && enabled->isChecked());
        QTest::mouseClick(enabled, Qt::LeftButton);
        verifyTag("hello", "eng", "word");
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(applied.count(), 3);
        verifyTag("hello", "unknown", "unknown");
        const auto saved = savedTaggers();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().toObject().value(QStringLiteral("language")).toString(),
                 QStringLiteral("eng"));
        QVERIFY(!saved.first().toObject().value(QStringLiteral("enabled")).toBool());
        auto *cancel = buttonWithText(dialog, LyricDialog::tr("&Cancel"));
        QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
    }

    LyricDialog reopened(singingClip, notes, singingClip->singerIdentifier(),
                         {TestSupport::fixtureLanguage()});
    openTaggerPage(reopened);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<FillLyric::TaggerConfigTab *>();
    auto *list = page->findChild<FillLyric::RuleListWidget *>();
    auto *details = page->findChild<FillLyric::TaggerDetailPanel *>();
    QVERIFY(list && details);
    auto *row = customRow(*list);
    QVERIFY(row);
    QCOMPARE(row->name(), QStringLiteral("eng"));
    QVERIFY(!row->isRuleEnabled());
    selectRule(*list, list->count() - 1);
    if (QTest::currentTestFailed())
        return;
    const auto editors = entryEditors(*details);
    QVERIFY(editors.language && editors.tag && editors.pattern);
    QCOMPARE(editors.language->currentText(), QStringLiteral("eng"));
    QCOMPARE(editors.tag->text(), QStringLiteral("word"));
    QCOMPARE(editors.pattern->toPlainText(), QStringLiteral("^hello$"));
    auto *enabled = row->findChild<QCheckBox *>();
    auto *apply = buttonWithText(*page, FillLyric::TaggerConfigTab::tr("Apply"));
    auto *remove = buttonWithText(*page, QStringLiteral("-"));
    QVERIFY(enabled && apply && remove && remove->isEnabled());
    QTest::mouseClick(enabled, Qt::LeftButton);
    QTest::mouseClick(apply, Qt::LeftButton);
    verifyTag("hello", "eng", "word");
    QCOMPARE(appOptions->fillLyric()->customTaggerRules.first().ruleId, savedRuleId);
    QTest::mouseClick(remove, Qt::LeftButton);
    verifyTag("hello", "eng", "word");
    QTest::mouseClick(apply, Qt::LeftButton);
    verifyTag("hello", "unknown", "unknown");
    QVERIFY(appOptions->fillLyric()->customTaggerRules.isEmpty());
    QVERIFY(!savedConfiguration().isEmpty());
    QVERIFY(savedTaggers().isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
}

void ApplicationGuiTests::invalidTaggerRegexPreservesAppliedRules() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().fillLyric;
    const auto engineOrder = currentEngineOrder();
    const auto restore = qScopeGuard([&] {
        QVERIFY(runtime.settings().updateFillLyric({}, original));
        FillLyric::TextTagger::setRuleOrder(engineOrder);
    });
    auto settings = original;
    isolateTaggers(settings);
    settings.customTaggerRules = {
        {
         .name = QStringLiteral("English words"),
         .language = QStringLiteral("eng"),
         .entries = {{.type = QStringLiteral("regex"),
                         .value = {QStringLiteral("^hello$")},
                         .tag = QStringLiteral("word")}},
         }
    };
    QVERIFY(runtime.settings().updateFillLyric({}, settings));
    const auto beforeSettings = runtime.settings().getSettings();
    QVERIFY(beforeSettings);
    const auto beforeFile = savedConfiguration();
    QVERIFY(!beforeFile.isEmpty());
    const auto before = runtime.documentVersion();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    QList<Note *> notes;
    for (auto *note : singingClip->notes())
        notes.append(note);
    {
        LyricDialog dialog(singingClip, notes, singingClip->singerIdentifier(),
                           {TestSupport::fixtureLanguage()});
        openTaggerPage(dialog);
        if (QTest::currentTestFailed())
            return;
        auto *page = dialog.findChild<FillLyric::TaggerConfigTab *>();
        auto *list = page->findChild<FillLyric::RuleListWidget *>();
        auto *details = page->findChild<FillLyric::TaggerDetailPanel *>();
        auto *apply = buttonWithText(*page, FillLyric::TaggerConfigTab::tr("Apply"));
        QVERIFY(list && details && apply);
        selectRule(*list, list->count() - 1);
        if (QTest::currentTestFailed())
            return;
        editRule(*details, QStringLiteral("cmn"), QStringLiteral("syllable"), QStringLiteral("["));
        if (QTest::currentTestFailed())
            return;
        QSignalSpy applied(page, &FillLyric::TaggerConfigTab::configChanged);
        bool dismissed = false;
        QTimer dismissError;
        dismissError.setSingleShot(true);
        connect(&dismissError, &QTimer::timeout, &dialog, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            const auto closeOnFailure = qScopeGuard([&] {
                if (!dismissed && modal)
                    modal->reject();
            });
            auto *message = qobject_cast<QMessageBox *>(modal);
            QVERIFY(message);
            QCOMPARE(message->windowTitle(), FillLyric::TaggerConfigTab::tr("Invalid Regex"));
            auto *ok = message->button(QMessageBox::Ok);
            QVERIFY(ok);
            QTest::mouseClick(ok, Qt::LeftButton);
            dismissed = true;
        });
        dismissError.start(0);
        QTest::mouseClick(apply, Qt::LeftButton);
        dismissError.stop();
        QVERIFY(dismissed);
        QCOMPARE(applied.count(), 0);
        verifyTag("hello", "eng", "word");
        const auto afterSettings = runtime.settings().getSettings();
        QVERIFY(afterSettings);
        QCOMPARE(afterSettings.get().fillLyric, beforeSettings.get().fillLyric);
        QCOMPARE(savedConfiguration(), beforeFile);
        auto *cancel = buttonWithText(dialog, LyricDialog::tr("&Cancel"));
        QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
    }
    LyricDialog reopened(singingClip, notes, singingClip->singerIdentifier(),
                         {TestSupport::fixtureLanguage()});
    openTaggerPage(reopened);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<FillLyric::TaggerConfigTab *>();
    auto *list = page->findChild<FillLyric::RuleListWidget *>();
    auto *details = page->findChild<FillLyric::TaggerDetailPanel *>();
    QVERIFY(list && details);
    selectRule(*list, list->count() - 1);
    if (QTest::currentTestFailed())
        return;
    const auto editors = entryEditors(*details);
    QVERIFY(editors.language && editors.pattern);
    QCOMPARE(editors.language->currentText(), QStringLiteral("eng"));
    QCOMPARE(editors.pattern->toPlainText(), QStringLiteral("^hello$"));
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
}
