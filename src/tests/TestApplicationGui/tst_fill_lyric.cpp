#include "tst_application_gui.h"
#include "../TestSupport/VoicebankFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/EditDialog.h"
#include "Modules/FillLyric/Controls/LyricCell.h"
#include "Modules/FillLyric/Controls/PhonicTextEdit.h"
#include "Modules/FillLyric/Widgets/RuleListItemWidget.h"
#include "Modules/FillLyric/Widgets/RuleListWidget.h"
#include "Modules/FillLyric/Widgets/RuleTestTab.h"
#include "Modules/FillLyric/Widgets/SplitterConfigTab.h"
#include "Modules/FillLyric/Widgets/SplitterDetailPanel.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScopeGuard>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    QPushButton *buttonWithText(QWidget *parent, const QString &text) {
        for (auto *button : parent->findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }

    template <typename Editor>
    void typeText(Editor *editor, const QString &text) {
        QVERIFY(editor);
        QWidget *clickTarget = editor;
        if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(editor))
            clickTarget = scrollArea->viewport();
        QTest::mouseClick(clickTarget, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QApplication::clipboard()->setText(text);
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keySequence(editor, QKeySequence::Paste);
    }

    void splitIntoPreview(LyricDialog &dialog, const QString &lyric) {
        auto *base = dialog.findChild<FillLyric::LyricBaseWidget *>();
        QVERIFY(base);
        auto *text = base->findChild<FillLyric::PhonicTextEdit *>();
        auto *previewButton = buttonWithText(base, QStringLiteral(">>"));
        QVERIFY(previewButton);
        typeText(text, lyric + QStringLiteral(" ") + lyric);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(text->toPlainText(), lyric + QStringLiteral(" ") + lyric);
        bool confirmed = false;
        QTimer confirmPreview;
        confirmPreview.setSingleShot(true);
        QObject::connect(&confirmPreview, &QTimer::timeout, &dialog, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *question = qobject_cast<QMessageBox *>(modal);
            if (!question && modal)
                modal->reject();
            QVERIFY(question);
            const auto rejectOnFailure = qScopeGuard([&] {
                if (!confirmed)
                    question->reject();
            });
            auto *yes = question->button(QMessageBox::Yes);
            QVERIFY(yes);
            QTest::mouseClick(yes, Qt::LeftButton);
            confirmed = true;
        });
        confirmPreview.start(0);
        QTest::mouseClick(previewButton, Qt::LeftButton);
        confirmPreview.stop();
        QVERIFY(confirmed);
        auto *preview = dialog.findChild<FillLyric::LyricWrapView *>();
        QVERIFY(preview);
        QCOMPARE(preview->cellLists().size(), 1);
        const auto cells = preview->cellLists().first()->m_cells;
        QCOMPARE(cells.size(), 2);
        QCOMPARE(cells.first()->lyric(), lyric);
        QVERIFY2(!cells.first()->syllable().isEmpty(),
                 "Preview must use the real language service");

        bool edited = false;
        auto *second = cells.at(1);
        const auto position = preview->mapFromScene(second->lyricRect().center());
        QVERIFY(preview->viewport()->rect().contains(position));
        QTimer editPreview;
        editPreview.setSingleShot(true);
        QObject::connect(&editPreview, &QTimer::timeout, &dialog, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activePopupWidget());
            if (!modal)
                modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *popup = dynamic_cast<FillLyric::EditDialog *>(modal);
            if (!popup && modal)
                modal->reject();
            QVERIFY(popup);
            const auto rejectOnFailure = qScopeGuard([&] {
                if (!edited)
                    popup->reject();
            });
            auto *editor = popup->findChild<QLineEdit *>();
            typeText(editor, QStringLiteral("-"));
            if (QTest::currentTestFailed())
                return;
            QTest::keyClick(editor, Qt::Key_Return);
            edited = true;
        });
        editPreview.start(0);
        QTest::mouseDClick(preview->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        editPreview.stop();
        QVERIFY(edited);
        QCOMPARE(second->lyric(), QStringLiteral("-"));
        const auto output = dialog.exportLangNotes();
        QCOMPARE(output.langNotes.size(), 2);
        QCOMPARE(output.langNotes.at(1).lyric, QStringLiteral("-"));
    }

    void selectLyricTab(LyricDialog &dialog, const QString &title) {
        auto *tabs = dialog.findChild<QTabWidget *>();
        QVERIFY(tabs);
        int selected = -1;
        for (int index = 0; index < tabs->count(); ++index) {
            if (tabs->tabText(index) == title)
                selected = index;
        }
        QVERIFY(selected >= 0);
        QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
                          tabs->tabBar()->tabRect(selected).center());
        QCOMPARE(tabs->currentIndex(), selected);
    }

    void runRulePreview(LyricDialog &dialog, QString &output) {
        selectLyricTab(dialog, LyricDialog::tr("Test"));
        if (QTest::currentTestFailed())
            return;
        auto *page = dialog.findChild<FillLyric::RuleTestTab *>();
        QVERIFY(page);
        auto *input = page->findChild<QPlainTextEdit *>("testInputEdit");
        auto *result = page->findChild<QPlainTextEdit *>("testOutputEdit");
        auto *run = buttonWithText(page, FillLyric::RuleTestTab::tr("Run Test"));
        QVERIFY(result);
        QVERIFY(run);
        typeText(input, QStringLiteral("lala"));
        if (QTest::currentTestFailed())
            return;
        QTest::mouseClick(run, Qt::LeftButton);
        QVERIFY(result->isReadOnly());
        output = result->toPlainText();
        QVERIFY(!output.isEmpty());
    }
}

void ApplicationGuiTests::createLyricSelection() {
    QTRY_COMPARE(appStatus->languageModuleStatus.get(), AppStatus::ModuleStatus::Ready);
    QTRY_COMPARE(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready);
    QTRY_COMPARE(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready);
    QVERIFY(!TestSupport::fixtureLanguage().isEmpty());
    QVERIFY(!TestSupport::fixtureLyric().isEmpty());
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY2(!singer.isEmpty(), "The configured fixture singer must be installed");
    QVERIFY(!singer.speakers().isEmpty());
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    singingClip->setDefaultLanguage(TestSupport::fixtureLanguage());
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.parameters().selectClipSingleSpeaker(commandContext(),
                                                         Automation::ClipId(singingClip->id()),
                                                         singer, singer.speakers().first()));
    QList<Automation::NoteDraftDto> notes;
    for (int index = 0; index < 3; ++index) {
        Automation::NoteDraftDto note;
        note.localStart = index * 480;
        note.length = 480;
        note.keyIndex = 60;
        note.lyric = TestSupport::fixtureLyric();
        note.language = TestSupport::fixtureLanguage();
        notes.append(note);
    }
    const auto inserted =
        runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()), notes);
    QVERIFY(inserted);
    QCOMPARE(singingClip->notes().size(), 3);
    QList<int> selected;
    for (const auto *note : singingClip->notes()) {
        if (selected.size() < 2)
            selected.append(note->id());
    }
    appStatus->selectedNotes = selected;
    historyManager->reset();
}

void ApplicationGuiTests::fillLyricPreviewCommitsOrCancels_data() {
    QTest::addColumn<bool>("accept");
    QTest::newRow("import") << true;
    QTest::newRow("cancel") << false;
}

void ApplicationGuiTests::fillLyricPreviewCommitsOrCancels() {
    QFETCH(bool, accept);
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().fillLyric;
    const auto restore = qScopeGuard([&] { runtime.settings().updateFillLyric({}, original); });
    auto settings = original;
    settings.baseVisible = true;
    settings.extensionVisible = true;
    settings.skipSlur = false;
    settings.splitMode = FillLyric::Auto;
    QVERIFY(runtime.settings().updateFillLyric({}, settings));
    const auto before = runtime.documentVersion();
    const auto selected = appStatus->selectedNotes.get();
    bool interacted = false;
    QTimer operateDialog;
    operateDialog.setInterval(10);
    connect(&operateDialog, &QTimer::timeout, this, [&] {
        auto *dialog = qobject_cast<LyricDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        operateDialog.stop();
        const auto cancelOnFailure = qScopeGuard([&] {
            if (!interacted)
                dialog->reject();
        });
        splitIntoPreview(*dialog, TestSupport::fixtureLyric());
        if (QTest::currentTestFailed())
            return;
        for (const auto *note : singingClip->notes())
            QCOMPARE(note->lyric(), TestSupport::fixtureLyric());
        auto *button = buttonWithText(dialog, LyricDialog::tr(accept ? "&Import" : "&Cancel"));
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        interacted = true;
    });
    operateDialog.start();
    clipController->onFillLyric(view.get());
    operateDialog.stop();
    QVERIFY(interacted);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(singingClip->notes().size(), 3);
    for (const auto *note : singingClip->notes()) {
        const auto expected = accept && note->id() == selected.at(1) ? QStringLiteral("-")
                                                                     : TestSupport::fixtureLyric();
        QCOMPARE(note->lyric(), expected);
    }
    if (accept) {
        QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        QVERIFY(historyManager->canUndo());
        historyManager->undo();
        for (const auto *note : singingClip->notes())
            QCOMPARE(note->lyric(), TestSupport::fixtureLyric());
        QVERIFY(!historyManager->canUndo());
        historyManager->redo();
        QCOMPARE(singingClip->findNoteById(selected.at(1))->lyric(), QStringLiteral("-"));
    } else {
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
    }
}

void ApplicationGuiTests::lyricRuleEditingChangesThePreviewAndPersists() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().fillLyric;
    const auto restore = qScopeGuard([&] { runtime.settings().updateFillLyric({}, original); });
    auto settings = original;
    settings.customSplitterRules.clear();
    settings.splitterOrder.clear();
    QVERIFY(runtime.settings().updateFillLyric({}, settings));
    const auto before = runtime.documentVersion();
    QList<Note *> notes;
    for (auto *note : singingClip->notes())
        notes.append(note);
    {
        LyricDialog dialog(singingClip, notes, singingClip->singerIdentifier(),
                           {TestSupport::fixtureLanguage()});
        dialog.show();
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isVisible());
        QString baseline;
        runRulePreview(dialog, baseline);
        if (QTest::currentTestFailed())
            return;
        selectLyricTab(dialog, LyricDialog::tr("Splitter"));
        if (QTest::currentTestFailed())
            return;
        auto *page = dialog.findChild<FillLyric::SplitterConfigTab *>();
        QVERIFY(page);
        auto *add = buttonWithText(page, QStringLiteral("+"));
        auto *apply = buttonWithText(page, FillLyric::SplitterConfigTab::tr("Apply"));
        QVERIFY(add);
        QVERIFY(apply);
        QTest::mouseClick(add, Qt::LeftButton);
        auto *details = page->findChild<FillLyric::SplitterDetailPanel *>();
        QVERIFY(details);
        auto *name = details->findChild<QLineEdit *>();
        QPlainTextEdit *pattern = nullptr;
        for (auto *editor : details->findChildren<QPlainTextEdit *>()) {
            if (!editor->isReadOnly())
                pattern = editor;
        }
        typeText(name, QStringLiteral("split-a"));
        typeText(pattern, QStringLiteral("(a)"));
        if (QTest::currentTestFailed())
            return;
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(appOptions->fillLyric()->customSplitterRules.size(), 1);
        QCOMPARE(appOptions->fillLyric()->customSplitterRules.first().name,
                 QStringLiteral("split-a"));
        QVERIFY(appOptions->fillLyric()->customSplitterRules.first().enabled);
        QString changed;
        runRulePreview(dialog, changed);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(changed != baseline);
        QVERIFY(changed.endsWith(QStringLiteral("l | a | l | a")));

        selectLyricTab(dialog, LyricDialog::tr("Splitter"));
        if (QTest::currentTestFailed())
            return;
        auto *list = page->findChild<FillLyric::RuleListWidget *>();
        QVERIFY(list);
        auto *item = list->item(list->count() - 1);
        list->scrollToItem(item);
        auto *row = qobject_cast<FillLyric::RuleListItemWidget *>(list->itemWidget(item));
        QVERIFY(row);
        QVERIFY(!row->isBuiltin());
        auto *enabled = row->findChild<QCheckBox *>();
        QVERIFY(enabled);
        QTest::mouseClick(enabled, Qt::LeftButton);
        QTest::mouseClick(apply, Qt::LeftButton);
        QVERIFY(!appOptions->fillLyric()->customSplitterRules.first().enabled);
        QString disabled;
        runRulePreview(dialog, disabled);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(disabled, baseline);
        auto *cancel = buttonWithText(&dialog, LyricDialog::tr("&Cancel"));
        QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
    }

    QFile file(appOptions->configPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto saved = QJsonDocument::fromJson(file.readAll())
                           .object()
                           .value(QStringLiteral("fillLyric"))
                           .toObject()
                           .value(QStringLiteral("customSplitterRules"))
                           .toArray();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("split-a"));
    QVERIFY(!saved.first().toObject().value(QStringLiteral("enabled")).toBool());
    LyricDialog reopened(singingClip, notes, singingClip->singerIdentifier(),
                         {TestSupport::fixtureLanguage()});
    reopened.show();
    reopened.activateWindow();
    selectLyricTab(reopened, LyricDialog::tr("Splitter"));
    if (QTest::currentTestFailed())
        return;
    FillLyric::RuleListItemWidget *savedRule = nullptr;
    for (auto *row : reopened.findChildren<FillLyric::RuleListItemWidget *>()) {
        if (row->name() == QStringLiteral("split-a"))
            savedRule = row;
    }
    QVERIFY(savedRule);
    QVERIFY(!savedRule->isRuleEnabled());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
