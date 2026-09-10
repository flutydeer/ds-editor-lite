#include "tst_editor_interaction.h"
#include "../TestSupport/GuiAppFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include "Modules/FillLyric/Widgets/RuleListItemWidget.h"
#include "Modules/FillLyric/Widgets/RuleListWidget.h"
#include "Modules/FillLyric/Widgets/SplitterConfigTab.h"
#include "Modules/FillLyric/Widgets/TaggerConfigTab.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TrackListView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

namespace {
    template <typename BeforeDrop>
    void dragListItem(QAbstractItemView &list, const QPoint &source, const QPoint &destination,
                      bool cancel, const BeforeDrop &beforeDrop) {
        QVERIFY(list.viewport()->rect().contains(source));
        QVERIFY(list.viewport()->rect().contains(destination));
        auto *window = list.window()->windowHandle();
        QVERIFY(window);
        const auto start = window->mapFromGlobal(list.viewport()->mapToGlobal(source));
        const auto finish = window->mapFromGlobal(list.viewport()->mapToGlobal(destination));
        const auto previousCursor = QCursor::pos();
        const auto restoreInput = qScopeGuard([&] {
            if (QGuiApplication::mouseButtons().testFlag(Qt::LeftButton))
                QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, finish);
            QCursor::setPos(previousCursor);
        });
        QCursor::setPos(list.viewport()->mapToGlobal(source));
        QTest::mouseMove(window, start);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTRY_COMPARE(list.currentIndex(), list.indexAt(source));
        QVERIFY(QGuiApplication::mouseButtons().testFlag(Qt::LeftButton));
        bool entered = false;
        QTimer finishDrag;
        finishDrag.setInterval(10);
        QObject::connect(&finishDrag, &QTimer::timeout, &list, [&] {
            if (!list.findChild<QDrag *>())
                return;
            finishDrag.stop();
            entered = true;
            const auto release = qScopeGuard([&] {
                if (cancel || QTest::currentTestFailed())
                    QTest::keyClick(window, Qt::Key_Escape);
                QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, finish);
            });
            QTest::mouseMove(window, finish);
            beforeDrop();
        });
        finishDrag.start();
        const int direction = finish.y() > start.y() ? 1 : -1;
        QTest::mouseMove(window,
                         start + QPoint(0, direction * (QApplication::startDragDistance() + 2)));
        finishDrag.stop();
        QVERIFY2(entered, "The handle gesture did not enter Qt's drag loop");
        QCoreApplication::processEvents();
    }
}

void EditorInteractionTests::trackListDragReordersOrCancels_data() {
    QTest::addColumn<int>("from");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("first-to-last") << 0 << false;
    QTest::newRow("last-to-first") << 2 << false;
    QTest::newRow("escape-preserves-order") << 0 << true;
}

void EditorInteractionTests::trackListDragReordersOrCancels() {
    QFETCH(int, from);
    QFETCH(bool, cancel);
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto &context = fixture.context;
    auto &runtime = *context->m_coreRuntime;
    const auto commandContext = [&] {
        return Automation::CommandContext{.expected = runtime.documentVersion(),
                                          .source = Automation::InvocationSource::Test};
    };
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    const QStringList names{QStringLiteral("First"), QStringLiteral("Second"),
                            QStringLiteral("Third")};
    for (int index = 0; index < names.size(); ++index) {
        Automation::ClipDraftDto clip;
        clip.properties.name = names.at(index);
        clip.properties.start = 480;
        clip.properties.length = 960;
        clip.properties.clipLen = 960;
        Automation::TrackDraftDto draft;
        draft.name = names.at(index);
        draft.clips.append(clip);
        QVERIFY(runtime.project().insertTrack(commandContext(), index, draft));
    }
    auto *list = editor.findChild<TrackListView *>();
    QVERIFY(list);
    editor.resize(1200, 550);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow() && list->isVisible());
    const auto trackIds = [&] {
        QList<int> ids;
        for (const auto *track : context->m_appModel->tracks())
            ids.append(track->id());
        return ids;
    };
    const auto original = trackIds();
    QHash<int, int> clipIds;
    for (const auto *track : context->m_appModel->tracks())
        clipIds.insert(track->id(), (*track->clips().begin())->id());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    auto *sourceControl = qobject_cast<TrackControlView *>(list->itemWidget(list->item(from)));
    QVERIFY(sourceControl);
    auto *handle = sourceControl->findChild<QLabel *>(QStringLiteral("lbTrackIndex"));
    QVERIFY(handle);
    QVERIFY(!handle->rect().isEmpty());
    QVERIFY(sourceControl->isInDragArea(handle->mapTo(sourceControl, handle->rect().center())));
    const auto source = handle->mapTo(list->viewport(), handle->rect().center());
    const auto targetRect = list->visualItemRect(list->item(from == 0 ? 2 : 0));
    const QPoint destination(source.x(),
                             from == 0 ? targetRect.bottom() - 2 : targetRect.top() + 2);
    dragListItem(*list, source, destination, cancel, [&] {
        QCOMPARE(trackIds(), original);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        auto *indicator = list->findChild<QWidget *>(QStringLiteral("trackDropIndicator"));
        QVERIFY(indicator && indicator->isVisible());
    });
    if (QTest::currentTestFailed())
        return;
    const auto expected = cancel      ? original
                          : from == 0 ? QList<int>{original.at(1), original.at(2), original.at(0)}
                                      : QList<int>{original.at(2), original.at(0), original.at(1)};
    QCOMPARE(trackIds(), expected);
    const auto verifyPresentation = [&] {
        QCOMPARE(list->trackCount(), context->m_appModel->tracks().size());
        for (int index = 0; index < list->trackCount(); ++index) {
            const auto *track = context->m_appModel->tracks().at(index);
            auto *control = qobject_cast<TrackControlView *>(list->itemWidget(list->item(index)));
            QVERIFY(control);
            QCOMPARE(control->id(), track->id());
            QCOMPARE(control->name(), track->name());
            QCOMPARE(control->trackIndex(), index + 1);
            const auto *clip = editor.findClipItemById(clipIds.value(track->id()));
            QVERIFY(clip);
            QCOMPARE(clip->trackIndex(), index);
        }
    };
    verifyPresentation();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!list->findChild<QWidget *>(QStringLiteral("trackDropIndicator"))->isVisible());
    if (cancel) {
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QTRY_COMPARE(list->currentRow(), from == 0 ? 2 : 0);
    historyManager->undo();
    QCOMPARE(trackIds(), original);
    verifyPresentation();
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QCOMPARE(trackIds(), expected);
    verifyPresentation();
}

void EditorInteractionTests::lyricRuleDragPreservesEditsAndChangesPriority_data() {
    QTest::addColumn<bool>("tagger");
    QTest::newRow("splitter") << false;
    QTest::newRow("tagger") << true;
}

void EditorInteractionTests::lyricRuleDragPreservesEditsAndChangesPriority() {
    QFETCH(bool, tagger);
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto &runtime = *fixture.context->m_coreRuntime;
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    auto settings = snapshot.get().fillLyric;
    const auto firstId = FillLyric::createAutomationRuleId();
    const auto secondId = FillLyric::createAutomationRuleId();
    if (tagger) {
        for (const auto &language : FillLyric::TextTagger::builtinLanguages())
            settings.builtinTaggerEnabled.insert(language, false);
        settings.customTaggerRules = {
            {.ruleId = firstId,
             .name = QStringLiteral("First"),
             .language = QStringLiteral("eng"),
             .entries = {{.type = QStringLiteral("array"),
                          .value = {QStringLiteral("abc")},
                          .tag = QStringLiteral("first")}},
             .enabled = true},
            {.ruleId = secondId,
             .name = QStringLiteral("Second"),
             .language = QStringLiteral("jpn"),
             .entries = {{.type = QStringLiteral("array"),
                          .value = {QStringLiteral("abc")},
                          .tag = QStringLiteral("second")}},
             .enabled = true}
        };
        settings.taggerOrder = {QStringLiteral("custom:eng"), QStringLiteral("custom:jpn")};
    } else {
        for (const auto &rule : FillLyric::TextSplitter::ruleInfoList()) {
            if (rule.builtin)
                settings.builtinSplitterEnabled.insert(rule.name, false);
        }
        settings.customSplitterRules = {
            {.ruleId = firstId,
             .name = QStringLiteral("First"),
             .regexes = {QStringLiteral("(ab)")},
             .enabled = true},
            {.ruleId = secondId,
             .name = QStringLiteral("Second"),
             .regexes = {QStringLiteral("(bc)")},
             .enabled = true}
        };
        settings.splitterOrder = {QStringLiteral("First"), QStringLiteral("Second")};
    }
    QVERIFY(runtime.settings().updateFillLyric({}, settings));
    const auto makePage = [&]() -> std::unique_ptr<QWidget> {
        if (tagger) {
            auto page = std::make_unique<FillLyric::TaggerConfigTab>();
            page->loadFromOption(appOptions->fillLyric());
            return page;
        }
        auto page = std::make_unique<FillLyric::SplitterConfigTab>();
        page->loadFromOption(appOptions->fillLyric());
        return page;
    };
    auto page = makePage();
    page->resize(1000, 550);
    page->show();
    page->activateWindow();
    QTRY_VERIFY(page->isActiveWindow());
    auto *list = page->findChild<FillLyric::RuleListWidget *>();
    QVERIFY(list);
    const auto rowName = [](FillLyric::RuleListWidget &view, int row) {
        const auto *widget =
            qobject_cast<FillLyric::RuleListItemWidget *>(view.itemWidget(view.item(row)));
        return widget ? widget->name() : QString{};
    };
    const auto firstName = tagger ? QStringLiteral("eng") : QStringLiteral("First");
    const auto secondName = tagger ? QStringLiteral("jpn") : QStringLiteral("Second");
    QCOMPARE(rowName(*list, 0), firstName);
    QCOMPARE(rowName(*list, 1), secondName);
    const auto *handle =
        list->itemWidget(list->item(0))->findChild<QLabel *>(QStringLiteral("dragHandle"));
    QVERIFY(handle);
    const auto source = handle->mapTo(list->viewport(), handle->rect().center());
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, source);
    QCOMPARE(list->currentRow(), 0);
    if (tagger) {
        QLineEdit *tag = nullptr;
        for (auto *edit : page->findChildren<QLineEdit *>()) {
            if (edit->isVisible() && edit->text() == QStringLiteral("first"))
                tag = edit;
        }
        QVERIFY(tag);
        QTest::mouseClick(tag, Qt::LeftButton);
        QTest::keySequence(tag, QKeySequence::SelectAll);
        QTest::keyClicks(tag, "edited");
        const auto tagged = FillLyric::TextTagger::tag({"abc"});
        QCOMPARE(tagged.size(), size_t{1});
        QCOMPARE(tagged.front().language, std::string("eng"));
        QCOMPARE(tagged.front().tag, std::string("first"));
    } else {
        QPlainTextEdit *pattern = nullptr;
        for (auto *edit : page->findChildren<QPlainTextEdit *>()) {
            if (edit->isVisible() && !edit->isReadOnly())
                pattern = edit;
        }
        QVERIFY(pattern);
        QTest::mouseClick(pattern->viewport(), Qt::LeftButton);
        QTest::keySequence(pattern, QKeySequence::SelectAll);
        QTest::keyClicks(pattern, "(ab|zz)");
        QCOMPARE(FillLyric::TextSplitter::split(std::string("abc")),
                 (std::vector<std::string>{"ab", "c"}));
    }
    const auto before = runtime.documentVersion();
    const auto beforeUndo = historyManager->nextUndoEntry();
    QSignalSpy applied(page.get(), SIGNAL(configChanged()));
    QVERIFY(applied.isValid());
    const auto destination = QPoint(source.x(), list->visualItemRect(list->item(1)).bottom() - 2);
    dragListItem(*list, source, destination, false, [&] {
        QCOMPARE(rowName(*list, 0), firstName);
        QCOMPARE(applied.size(), 0);
    });
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(rowName(*list, 0), secondName);
    QCOMPARE(rowName(*list, 1), firstName);
    QCOMPARE(applied.size(), 0);
    QPushButton *apply = nullptr;
    for (auto *button : page->findChildren<QPushButton *>()) {
        if (button->text() == (tagger ? FillLyric::TaggerConfigTab::tr("Apply")
                                      : FillLyric::SplitterConfigTab::tr("Apply")))
            apply = button;
    }
    QVERIFY(apply);
    QTest::mouseClick(apply, Qt::LeftButton);
    QCOMPARE(applied.size(), 1);
    AppOptions reopened;
    if (tagger) {
        const auto &rules = reopened.fillLyric()->customTaggerRules;
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules.at(0).ruleId, secondId);
        QCOMPARE(rules.at(1).ruleId, firstId);
        QCOMPARE(rules.at(1).entries.size(), 1);
        QCOMPARE(rules.at(1).entries.first().tag, QStringLiteral("edited"));
        const auto tagged = FillLyric::TextTagger::tag({"abc"});
        QCOMPARE(tagged.size(), size_t{1});
        QCOMPARE(tagged.front().language, std::string("jpn"));
        QCOMPARE(tagged.front().tag, std::string("second"));
    } else {
        const auto &rules = reopened.fillLyric()->customSplitterRules;
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules.at(0).ruleId, secondId);
        QCOMPARE(rules.at(1).ruleId, firstId);
        QCOMPARE(rules.at(1).regexes, QStringList{QStringLiteral("(ab|zz)")});
        QCOMPARE(FillLyric::TextSplitter::split(std::string("abc")),
                 (std::vector<std::string>{"a", "bc"}));
        QCOMPARE(FillLyric::TextSplitter::split(std::string("zzx")),
                 (std::vector<std::string>{"zz", "x"}));
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
    page.reset();
    page = makePage();
    list = page->findChild<FillLyric::RuleListWidget *>();
    QVERIFY(list);
    QCOMPARE(rowName(*list, 0), secondName);
    QCOMPARE(rowName(*list, 1), firstName);
}
