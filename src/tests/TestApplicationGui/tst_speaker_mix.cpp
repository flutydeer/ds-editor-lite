#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixBar.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixList.h"
#include "UI/Controls/TwoLevelComboBox.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "Controller/TrackController.h"
#include "Utils/UiLanguageManager.h"
#include "../TestSupport/VoicebankFixture.h"
#include "../TestSupport/PointerInput.h"
#include "../TestSupport/WaveFixture.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/TagButton.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QJsonArray>
#include <QSignalSpy>
#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScopeGuard>
#include <QTimer>
#include <QWindow>
#include <QPointer>
#include <QTemporaryDir>
#include <QEvent>
#include <QImage>
#include <QtTest/QTest>

#include <lite/GUI/Theme/ThemeManager.h>

namespace {
    QJsonArray savedPresets() {
        QJsonArray result;
        for (const auto &preset : SpeakerMixPresetStore::allPresets())
            result.append(preset.toJson());
        return result;
    }

    TagButton *speakerTag(SpeakerMixDialog &dialog, const QString &id) {
        for (auto *tag : dialog.findChildren<TagButton *>()) {
            if (tag->property("speakerName").toString() == id)
                return tag;
        }
        return nullptr;
    }

    QPushButton *mixButton(QWidget &dialog, const QString &text) {
        for (auto *button : dialog.findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }
}

void ApplicationGuiTests::clipToolbarNameEditingKeepsTheOriginalTarget_data() {
    QTest::addColumn<bool>("secondAudio");
    QTest::newRow("singing-to-singing") << false;
    QTest::newRow("singing-to-audio") << true;
}

void ApplicationGuiTests::clipToolbarNameEditingKeepsTheOriginalTarget() {
    QFETCH(bool, secondAudio);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    auto &runtime = *context->m_coreRuntime;
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::TrackDraftDto track;
    for (const auto &name : {QStringLiteral("First clip"), QStringLiteral("Second clip")}) {
        Automation::ClipDraftDto clip;
        clip.properties.name = name;
        clip.properties.length = 1920;
        clip.properties.clipLen = 1920;
        if (secondAudio && name == QStringLiteral("Second clip")) {
            clip.type = Automation::ClipDraftDto::Type::Audio;
            clip.audioPath = files.filePath(QStringLiteral("toolbar.wav"));
            QVERIFY(TestSupport::writeWave(clip.audioPath, QVector<float>(4800, 0.125f)));
        }
        track.clips.append(clip);
    }
    document.tracks = {track};
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), document));
    const auto releaseAudio = qScopeGuard([&] {
        if (!secondAudio)
            return;
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    Clip *first = nullptr;
    Clip *second = nullptr;
    for (auto *clip : context->m_appModel->tracks().first()->clips()) {
        if (clip->name() == QStringLiteral("First clip"))
            first = clip;
        else if (clip->name() == QStringLiteral("Second clip"))
            second = clip;
    }
    QVERIFY(first && second);
    ClipEditorToolBarView toolbar;
    toolbar.setDataContext(first);
    toolbar.resize(1100, 48);
    toolbar.show();
    toolbar.activateWindow();
    auto *label = toolbar.findChild<InlineEditLabel *>("leClipName");
    auto *language = toolbar.findChild<LanguageComboBox *>();
    auto *noteTool = toolbar.findChild<QAbstractButton *>("btnNotePencil");
    QVERIFY(label && language && noteTool);
    QTRY_VERIFY(toolbar.isActiveWindow() && label->isVisible() && label->isEnabled());
    QVERIFY(language->isVisible() && language->isEnabled());
    QVERIFY(noteTool->isVisible() && noteTool->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto edit = [&](const char *text) {
        QTest::mouseDClick(label, Qt::LeftButton);
        QTRY_VERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
        auto *input = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        QTest::keySequence(input, QKeySequence::SelectAll);
        QTest::keyClicks(input, text);
    };
    edit("Canceled name");
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    QCOMPARE(first->name(), QStringLiteral("First clip"));
    QCOMPARE(label->text(), first->name());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    edit("Committed before switching");
    if (QTest::currentTestFailed())
        return;
    toolbar.setDataContext(second);
    QCOMPARE(first->name(), QStringLiteral("Committed before switching"));
    QCOMPARE(second->name(), QStringLiteral("Second clip"));
    QCOMPARE(label->text(), second->name());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(language->isVisible(), !secondAudio);
    QCOMPARE(language->isEnabled(), !secondAudio);
    QCOMPARE(noteTool->isVisible(), !secondAudio);
    QCOMPARE(noteTool->isEnabled(), !secondAudio);
    edit("Edited second clip");
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
    QCOMPARE(second->name(), QStringLiteral("Edited second clip"));
    QCOMPARE(label->text(), second->name());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(second->name(), QStringLiteral("Second clip"));
    QCOMPARE(first->name(), QStringLiteral("Committed before switching"));
    QCOMPARE(label->text(), second->name());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(first->name(), QStringLiteral("First clip"));
    QCOMPARE(label->text(), second->name());
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
    const auto beforeReturn = runtime.documentVersion();
    const auto beforeReturnModel = TestSupport::projectSnapshot(*context->m_appModel);
    toolbar.setDataContext(first);
    QCOMPARE(label->text(), first->name());
    QVERIFY(language->isVisible() && language->isEnabled());
    QVERIFY(noteTool->isVisible() && noteTool->isEnabled());
    QCOMPARE(runtime.documentVersion(), beforeReturn);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeReturnModel);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::voiceMenusApplyPresetsToTheChosenTarget_data() {
    QTest::addColumn<bool>("clipTarget");
    QTest::newRow("clip-toolbar") << true;
    QTest::newRow("track-header") << false;
}

void ApplicationGuiTests::voiceMenusApplyPresetsToTheChosenTarget() {
    QFETCH(bool, clipTarget);
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY(!singer.isEmpty());
    if (singer.speakers().size() < 2)
        QSKIP("The selected voicebank needs two speakers for the mix preset menu");
    const auto first = singer.speakers().first();
    const auto second = singer.speakers().at(1);
    auto &runtime = *context->m_coreRuntime;
    std::unique_ptr<QWidget> host;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    if (!clipTarget)
        host = std::make_unique<TrackEditorView>();
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::ClipDraftDto clip;
    clip.properties.name = QStringLiteral("Voice menu target");
    clip.properties.length = 1920;
    clip.properties.clipLen = 1920;
    clip.defaultLanguage = TestSupport::fixtureLanguage();
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Inherited voice");
    track.defaultLanguage = TestSupport::fixtureLanguage();
    track.singerInfo = singer;
    track.speakerInfo = first;
    track.clips = {clip};
    document.tracks = {track};
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), document));
    auto *modelTrack = context->m_appModel->tracks().first();
    auto *modelClip = qobject_cast<SingingClip *>(*modelTrack->clips().begin());
    QVERIFY(modelClip && modelClip->usesTrackVoiceContext());
    SpeakerMixPreset preset;
    preset.name = QStringLiteral("Menu blend");
    preset.packageId = singer.packageId();
    preset.packageVersion = singer.packageVersion();
    preset.singerId = singer.singerId();
    preset.sources = {{first}, {second}};
    preset.fixedWeights = {0.3};
    const auto saved = SpeakerMixPresetStore::savePreset(preset);
    QVERIFY(saved);
    const auto cleanup =
        qScopeGuard([&] { QVERIFY(SpeakerMixPresetStore::deletePreset(saved->id)); });
    if (clipTarget) {
        auto toolbar = std::make_unique<ClipEditorToolBarView>();
        toolbar->setDataContext(modelClip);
        toolbar->resize(1100, 48);
        host = std::move(toolbar);
    } else {
        host->resize(1100, 400);
    }
    auto *combo = host->findChild<TwoLevelComboBox *>();
    QVERIFY(combo);
    host->show();
    host->activateWindow();
    QTRY_VERIFY(host->isActiveWindow() && combo->isVisible() && combo->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto choose = [&](const QString &text, bool followTrack = false) {
        bool chosen = false;
        QTimer::singleShot(0, combo, [&] {
            auto *root = combo->mainMenu();
            auto *group = combo->groupMenuForSinger(singer);
            const auto close = qScopeGuard([&] { root->close(); });
            QTRY_VERIFY(root->isVisible());
            QMenu *menu = root;
            if (!followTrack) {
                QVERIFY(group);
                QTest::mouseClick(root, Qt::LeftButton, Qt::NoModifier,
                                  root->actionGeometry(group->menuAction()).center());
                QTRY_VERIFY(group->isVisible());
                menu = group;
            }
            QAction *choice = nullptr;
            for (auto *action : menu->actions()) {
                if (action->text() == text)
                    choice = action;
            }
            QVERIFY(choice && choice->isEnabled());
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(choice).center());
            chosen = true;
        });
        QTest::mouseClick(combo, Qt::LeftButton);
        QVERIFY(chosen);
    };
    const auto mix = [&] {
        return clipTarget ? modelClip->speakerMixData() : modelTrack->speakerMixData();
    };
    choose(saved->name);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(mix().mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    QCOMPARE(mix().fixedWeights, QVector<double>{0.3});
    QCOMPARE(mix().sourcePresetId, saved->id);
    QVERIFY(combo->currentText().contains(saved->name));
    QCOMPARE(modelClip->usesTrackVoiceContext(), !clipTarget);
    if (clipTarget)
        QCOMPARE(modelTrack->speakerMixData().mode, SpeakerMixModel::SingerSourceMode::Single);
    else
        QCOMPARE(modelClip->speakerMixData().fixedWeights, QVector<double>{0.3});

    const auto presetVersion = runtime.documentVersion();
    const auto *presetEdit = historyManager->nextUndoEntry();
    for (const bool accept : {false, true}) {
        bool inspected = false;
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, host.get(), [&] {
            auto *dialog = qobject_cast<SpeakerMixDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            answer.stop();
            const auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto *list = dialog->findChild<SpeakerMixList *>();
            QVERIFY(list);
            QCOMPARE(list->getValues(), QVector<int>({30, 70}));
            if (accept)
                QTest::mouseClick(dialog->okButton(), Qt::LeftButton);
            else
                QTest::mouseClick(dialog->cancelButton(), Qt::LeftButton);
            inspected = true;
        });
        answer.start();
        choose(QCoreApplication::translate(clipTarget ? "ClipEditorToolBarViewPrivate"
                                                      : "TrackControlView",
                                           "Manage mix presets..."));
        if (QTest::currentTestFailed())
            return;
        QVERIFY(inspected);
        QCOMPARE(runtime.documentVersion(), presetVersion);
        QCOMPARE(historyManager->nextUndoEntry(), presetEdit);
        QCOMPARE(mix().sourcePresetId, saved->id);
        QVERIFY(combo->currentText().contains(saved->name));
    }

    choose(second.displayName(UiLanguageManager::currentBcp47Candidates()));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(modelClip->speakerInfo().id(), second.id());
    QCOMPARE(combo->currentSpeaker().id(), second.id());
    QCOMPARE(mix().mode, SpeakerMixModel::SingerSourceMode::Single);
    if (clipTarget) {
        QCOMPARE(modelTrack->speakerInfo().id(), first.id());
        choose(TwoLevelComboBox::tr("Follow Track"), true);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(modelClip->usesTrackVoiceContext());
        QVERIFY(combo->isInheritSelected());
        QCOMPARE(modelClip->speakerInfo().id(), first.id());
        QVERIFY(runtime.history().undo(commandContext()));
        QVERIFY(!modelClip->usesTrackVoiceContext());
        QCOMPARE(modelClip->speakerInfo().id(), second.id());
    }
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(mix().mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    QVERIFY(combo->currentText().contains(saved->name));
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(modelClip->speakerInfo().id(), first.id());
    QVERIFY(modelClip->usesTrackVoiceContext());
    QCOMPARE(mix().mode, SpeakerMixModel::SingerSourceMode::Single);
    QVERIFY(!historyManager->canUndo());
    if (clipTarget) {
        auto *language = host->findChild<LanguageComboBox *>();
        QVERIFY(language && language->isVisible() && language->isEnabled());
        const auto previousLanguage = modelClip->defaultLanguage();
        const auto previousIndex = language->currentIndex();
        const auto otherIndex = previousIndex == 0 ? 1 : 0;
        QVERIFY(otherIndex < language->count());
        const auto chosenLanguage = language->itemData(otherIndex).toString();
        QVERIFY(!chosenLanguage.isEmpty() && chosenLanguage != previousLanguage);
        const auto beforeLanguage = runtime.documentVersion();
        const auto beforeLanguageModel = TestSupport::projectSnapshot(*context->m_appModel);
        QTest::mouseClick(language, Qt::LeftButton);
        QTRY_VERIFY(language->view()->isVisible());
        QTest::keyClick(language->view(), Qt::Key_Home);
        if (otherIndex == 1)
            QTest::keyClick(language->view(), Qt::Key_Down);
        QTest::keyClick(language->view(), Qt::Key_Return);
        QCOMPARE(modelClip->defaultLanguage(), chosenLanguage);
        QCOMPARE(language->currentLanguage(), chosenLanguage);
        QCOMPARE(runtime.documentVersion().revision, beforeLanguage.revision + 1);
        QVERIFY(runtime.history().undo(commandContext()));
        QCOMPARE(modelClip->defaultLanguage(), previousLanguage);
        QCOMPARE(language->currentLanguage(), previousLanguage);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeLanguageModel);
        QVERIFY(!historyManager->canUndo());
    }
}

void ApplicationGuiTests::speakerMixSelectionAndDrag_data() {
    QTest::addColumn<bool>("accept");
    QTest::newRow("accept-edited-mix") << true;
    QTest::newRow("cancel-edited-mix") << false;
}

void ApplicationGuiTests::speakerMixModifierDragPreservesGroupRatios() {
    SpeakerMixBar bar;
    bar.setValues({20, 30, 50});
    bar.resize(402, 40);
    bar.show();
    bar.activateWindow();
    QTRY_VERIFY(bar.isActiveWindow());
    const auto before = context->m_coreRuntime->documentVersion();
    QSignalSpy changed(&bar, &SpeakerMixBar::valuesChanged);
    const QPoint divider(bar.width() / 2, bar.height() / 2);
    const QPoint destination(1 + qRound((bar.width() - 2) * 0.75), divider.y());
    const auto release = qScopeGuard(
        [&] { QTest::mouseRelease(&bar, Qt::LeftButton, Qt::AltModifier, destination); });
    QTest::mousePress(&bar, Qt::LeftButton, Qt::AltModifier, divider);
    QMouseEvent move(QEvent::MouseMove, QPointF(destination), QPointF(bar.mapToGlobal(destination)),
                     Qt::NoButton, Qt::LeftButton, Qt::AltModifier);
    QApplication::sendEvent(&bar, &move);
    QVERIFY(!changed.isEmpty());
    QCOMPARE(bar.getValues(), QVector<int>({30, 45, 25}));
    QCOMPARE(changed.last().first().value<QVector<double>>(), QVector<double>({30, 45, 25}));
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::speakerMixSelectionAndDrag() {
    QFETCH(bool, accept);
    using namespace SpeakerMixModel;
    const SingerInfo singer(
        {QStringLiteral("mix-test"), QStringLiteral("gui-tests"), QVersionNumber(1, 0)},
        QStringLiteral("Mix Test"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm")),
         SpeakerInfo(QStringLiteral("air"), QStringLiteral("Air"))});
    const auto before = context->m_coreRuntime->documentVersion();
    const auto presets = savedPresets();
    SpeakerMixDialog dialog(singer, {});
    auto *theme = ThemeManager::instance();
    const auto originalTheme = theme->currentThemeId();
    theme->addStyleRoot(&dialog);
    const auto restoreTheme = qScopeGuard([&] {
        QVERIFY(theme->applyTheme(originalTheme));
        theme->removeStyleRoot(&dialog);
    });
    auto *list = dialog.findChild<SpeakerMixList *>();
    auto *air = speakerTag(dialog, QStringLiteral("air"));
    QVERIFY(list);
    QVERIFY(air);
    auto *bar = list->getMixBar();
    QVERIFY(bar);
    QSignalSpy finished(&dialog, &QDialog::finished);
    QSignalSpy valuesChanged(bar, &SpeakerMixBar::valuesChanged);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(air->isVisible());
    QTRY_VERIFY(bar->isVisible());
    QVERIFY(air->isChecked());

    QTest::mouseClick(air, Qt::LeftButton);
    QVERIFY(!air->isChecked());
    QCOMPARE(list->getLabels(),
             QVector<QString>({QStringLiteral("bright"), QStringLiteral("warm")}));
    // Removing a source preserves the remaining 34:33 ratio.
    QCOMPARE(list->getValues(), QVector<int>({51, 49}));
    const QPoint divider(1 + qRound((bar->width() - 2) * bar->getDoubleValues().first() / 100.0),
                         bar->height() / 2);
    const QPoint destination(1 + qRound((bar->width() - 2) * 0.70), divider.y());
    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, divider);
    QTest::mouseMove(bar, destination);
    QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, destination);
    QVERIFY(!valuesChanged.isEmpty());
    QCOMPARE(bar->getValues(), QVector<int>({70, 30}));
    QCOMPARE(list->getValues(), QVector<int>({70, 30}));

    const auto labels = list->getLabels();
    const auto weights = bar->getDoubleValues();
    const auto notifications = valuesChanged.size();
    QImage previousFrame;
    for (const auto &id : {QStringLiteral("lite-light"), QStringLiteral("lite-dark")}) {
        QVERIFY(theme->applyTheme(id));
        QTRY_COMPARE(bar->property("trackColor").value<QColor>().rgba(),
                     theme->semanticColor(QStringLiteral("speakerMix.track")).rgba());
        QCOMPARE(bar->property("segmentTextColor").value<QColor>().rgba(),
                 theme->semanticColor(QStringLiteral("speakerMix.segmentText")).rgba());
        QCOMPARE(bar->property("dividerColor").value<QColor>().rgba(),
                 theme->semanticColor(QStringLiteral("speakerMix.divider")).rgba());
        QCOMPARE(bar->property("dividerDraggingColor").value<QColor>().rgba(),
                 theme->semanticColor(QStringLiteral("speakerMix.dividerDragging")).rgba());
        const auto frame = bar->grab().toImage();
        QVERIFY(!frame.isNull());
        if (!previousFrame.isNull())
            QVERIFY(frame != previousFrame);
        previousFrame = frame;
        QCOMPARE(list->getLabels(), labels);
        QCOMPARE(bar->getDoubleValues(), weights);
        QCOMPARE(valuesChanged.size(), notifications);
        QCOMPARE(context->m_coreRuntime->documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
    }

    if (accept)
        QTest::mouseClick(dialog.okButton(), Qt::LeftButton);
    else
        QTest::mouseClick(dialog.cancelButton(), Qt::LeftButton);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toInt(), accept ? QDialog::Accepted : QDialog::Rejected);
    QVERIFY(!dialog.isVisible());
    if (accept) {
        const auto result = dialog.speakerMixData();
        QCOMPARE(result.mode, SingerSourceMode::FixedMix);
        QCOMPARE(result.sources.size(), 2);
        QCOMPARE(result.sources[0].speaker.id(), QStringLiteral("bright"));
        QCOMPARE(result.sources[1].speaker.id(), QStringLiteral("warm"));
        QCOMPARE(result.fixedWeights, QVector<double>({0.7}));
    } else {
        SpeakerMixDialog reopened(singer, {});
        auto *reopenedList = reopened.findChild<SpeakerMixList *>();
        QVERIFY(reopenedList);
        QCOMPARE(reopenedList->getLabels(),
                 QVector<QString>(
                     {QStringLiteral("bright"), QStringLiteral("warm"), QStringLiteral("air")}));
        QCOMPARE(reopenedList->getValues(), QVector<int>({34, 33, 33}));
    }
    QCOMPARE(savedPresets(), presets);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::speakerMixPresetsFollowSaveSelectAndDeleteInputs() {
    const auto nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard(
        [&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled); });
    const SingerInfo singer(
        {QStringLiteral("preset-test"), QStringLiteral("gui-tests"), QVersionNumber(1, 0)},
        QStringLiteral("Preset Test"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm")),
         SpeakerInfo(QStringLiteral("air"), QStringLiteral("Air"))});
    const auto before = context->m_coreRuntime->documentVersion();
    const auto originalPresets = savedPresets();
    QString savedId;
    const auto cleanup = qScopeGuard([&] {
        if (!savedId.isEmpty())
            SpeakerMixPresetStore::deletePreset(savedId);
    });
    SpeakerMixDialog dialog(singer, {});
    auto *presets = dialog.findChild<QComboBox *>();
    auto *save = mixButton(dialog, SpeakerMixDialog::tr("Save As"));
    auto *more = mixButton(dialog, SpeakerMixDialog::tr("More..."));
    auto *list = dialog.findChild<SpeakerMixList *>();
    QVERIFY(presets);
    QVERIFY(save);
    QVERIFY(more);
    QVERIFY(list);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    dialog.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&dialog));

    QString step;
    QString phase;
    QPointer<QDialog> activePrompt;
    QPointer<QMenu> activeMenu;
    QTimer watchdog;
    // Bound synchronous confirmations and clean up only this test's windows.
    watchdog.setSingleShot(true);
    connect(&watchdog, &QTimer::timeout, &dialog, [&] {
        const auto describe = [](QWidget *widget) {
            if (!widget)
                return QStringLiteral("none");
            return QStringLiteral("%1 visible=%2 exposed=%3 active=%4")
                .arg(QString::fromLatin1(widget->metaObject()->className()))
                .arg(widget->isVisible())
                .arg(widget->windowHandle() && widget->windowHandle()->isExposed())
                .arg(widget->isActiveWindow());
        };
        const auto diagnostic =
            QStringLiteral("Preset input did not finish: %1; phase=%2; menu=%3; prompt=%4; "
                           "popup=%5; modal=%6")
                .arg(step, phase, describe(activeMenu), describe(activePrompt),
                     describe(QApplication::activePopupWidget()),
                     describe(QApplication::activeModalWidget()));
        if (activeMenu)
            activeMenu->close();
        if (activePrompt)
            activePrompt->reject();
        QFAIL(qPrintable(diagnostic));
    });

    const auto saveAs = [&](bool accept) {
        bool visited = false;
        step = accept ? QStringLiteral("save preset") : QStringLiteral("cancel saving preset");
        phase = QStringLiteral("waiting for name prompt");
        watchdog.start(5000);
        QTimer input;
        input.setInterval(10);
        connect(&input, &QTimer::timeout, &dialog, [&] {
            auto *prompt = dialog.findChild<QInputDialog *>();
            if (!prompt || !prompt->isVisible())
                return;
            activePrompt = prompt;
            if (!prompt->windowHandle() || !prompt->windowHandle()->isExposed())
                return;
            input.stop();
            phase = QStringLiteral("entering preset name");
            const auto close = qScopeGuard([&] {
                if (prompt->isVisible())
                    prompt->reject();
            });
            auto *editor = prompt->findChild<QLineEdit *>();
            QVERIFY(editor);
            QTest::mouseClick(editor, Qt::LeftButton);
            QTest::keyClicks(editor, "Saved GUI mix");
            QTest::keyClick(editor, accept ? Qt::Key_Return : Qt::Key_Escape);
            visited = true;
        });
        input.start();
        QTest::mouseClick(save, Qt::LeftButton);
        watchdog.stop();
        activePrompt.clear();
        QVERIFY(visited);
    };
    saveAs(false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(savedPresets(), originalPresets);
    saveAs(true);
    if (QTest::currentTestFailed())
        return;
    savedId = presets->currentData().toString();
    QVERIFY(!savedId.isEmpty());
    const auto saved = SpeakerMixPresetStore::findPreset(savedId);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Saved GUI mix"));
    QCOMPARE(saved->sources.size(), 3);

    auto *air = speakerTag(dialog, QStringLiteral("air"));
    QVERIFY(air);
    QTest::mouseClick(air, Qt::LeftButton);
    QTest::mouseClick(dialog.okButton(), Qt::LeftButton);
    const auto edited = dialog.speakerMixData();
    QCOMPARE(edited.sourcePresetId, savedId);
    QVERIFY(edited.sourcePresetDirty);
    QCOMPARE(edited.sources.size(), 2);

    SpeakerMixDialog reopened(singer, edited);
    presets = reopened.findChild<QComboBox *>();
    list = reopened.findChild<SpeakerMixList *>();
    more = mixButton(reopened, SpeakerMixDialog::tr("More..."));
    QVERIFY(presets);
    QVERIFY(list);
    QVERIFY(more);
    reopened.show();
    QVERIFY(QTest::qWaitForWindowExposed(&reopened));
    reopened.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&reopened));
    QCOMPARE(presets->currentData().toString(), savedId);
    QTest::mouseClick(presets, Qt::LeftButton);
    QTRY_VERIFY(presets->view()->isVisible());
    QTest::keyClick(presets->view(), Qt::Key_Home);
    QTest::keyClick(presets->view(), Qt::Key_Return);
    QCOMPARE(presets->currentIndex(), 0);
    QTest::mouseClick(presets, Qt::LeftButton);
    QTRY_VERIFY(presets->view()->isVisible());
    QTest::keyClick(presets->view(), Qt::Key_End);
    QTest::keyClick(presets->view(), Qt::Key_Return);
    QCOMPARE(presets->currentData().toString(), savedId);
    QCOMPARE(list->getLabels().size(), 3);
    QCOMPARE(list->getValues(), QVector<int>({34, 33, 33}));

    const auto choosePresetAction = [&](const QString &label,
                                        QMessageBox::StandardButton response) {
        bool visited = false;
        step = QStringLiteral("%1 (%2)").arg(label).arg(response);
        phase = QStringLiteral("opening preset menu");
        auto *menu = more->findChild<QMenu *>();
        QVERIFY(menu);
        activeMenu = menu;
        const auto closeMenu = qScopeGuard([&] {
            menu->close();
            activeMenu.clear();
        });
        TestSupport::hoverWidget(*more, more->rect().center());
        QTest::mouseClick(more, Qt::LeftButton);
        QTRY_VERIFY(menu->isVisible() && QApplication::activePopupWidget() == menu);
        QVERIFY(QTest::qWaitForWindowExposed(menu));
        QAction *selected = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == label) {
                selected = action;
                break;
            }
        }
        QVERIFY2(selected,
                 qPrintable(QStringLiteral("Preset action was not found: %1").arg(label)));
        QVERIFY(selected->isEnabled());
        phase = QStringLiteral("hovering preset action");
        const auto actionRect = menu->actionGeometry(selected);
        TestSupport::hoverWidget(*menu, QPoint(actionRect.left() + 1, actionRect.center().y()));
        TestSupport::hoverWidget(*menu, actionRect.center());
        QTRY_COMPARE(menu->activeAction(), selected);
        QSignalSpy triggered(selected, &QAction::triggered);
        QTimer confirmation;
        confirmation.setInterval(10);
        connect(&confirmation, &QTimer::timeout, &reopened, [&] {
            auto *prompt = reopened.findChild<QMessageBox *>();
            if (!prompt || !prompt->isVisible())
                return;
            activePrompt = prompt;
            phase = QStringLiteral("waiting for confirmation exposure");
            if (!prompt->windowHandle() || !prompt->windowHandle()->isExposed())
                return;
            confirmation.stop();
            phase = QStringLiteral("clicking confirmation");
            const auto close = qScopeGuard([&] {
                if (prompt->isVisible())
                    prompt->reject();
            });
            auto *button = prompt->button(response);
            QVERIFY(button);
            QVERIFY(button->isVisible() && button->isEnabled());
            QSignalSpy clicked(button, &QAbstractButton::clicked);
            QTest::mouseClick(button, Qt::LeftButton);
            QVERIFY(!clicked.isEmpty());
            QVERIFY(!prompt->isVisible());
            visited = true;
        });
        phase = QStringLiteral("clicking preset action");
        confirmation.start();
        watchdog.start(5000);
        // Keep the click together after the native hover queue has settled.
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, actionRect.center());
        QTRY_VERIFY(visited || QTest::currentTestFailed());
        confirmation.stop();
        watchdog.stop();
        activePrompt.clear();
        if (QTest::currentTestFailed())
            return;
        QVERIFY(visited);
        QCOMPARE(triggered.size(), 1);
        QVERIFY(!menu->isVisible());
    };
    auto *invert = mixButton(reopened, SpeakerMixDialog::tr("Invert"));
    auto *all = mixButton(reopened, SpeakerMixDialog::tr("All"));
    QVERIFY(invert && all);
    QTest::mouseClick(invert, Qt::LeftButton);
    QCOMPARE(list->getLabels().size(), 1);
    const auto onlySource = list->getLabels().first();
    auto *onlyTag = speakerTag(reopened, onlySource);
    QVERIFY(onlyTag && onlyTag->isChecked());
    QTest::mouseClick(onlyTag, Qt::LeftButton);
    QVERIFY(onlyTag->isChecked());
    QCOMPARE(list->getLabels(), QVector<QString>{onlySource});
    QTest::mouseClick(all, Qt::LeftButton);
    QCOMPARE(list->getLabels().size(), singer.speakers().size());
    QTest::mouseClick(invert, Qt::LeftButton);
    QCOMPARE(list->getLabels().size(), 1);
    const auto beforeReset = list->getLabels();
    choosePresetAction(SpeakerMixDialog::tr("Initialize"), QMessageBox::No);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(list->getLabels(), beforeReset);
    QCOMPARE(presets->currentData().toString(), savedId);
    choosePresetAction(SpeakerMixDialog::tr("Initialize"), QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(presets->currentIndex(), 0);
    QCOMPARE(list->getLabels(), QVector<QString>({QStringLiteral("bright"), QStringLiteral("warm"),
                                                  QStringLiteral("air")}));
    QCOMPARE(list->getValues(), QVector<int>({34, 33, 33}));
    QVERIFY(SpeakerMixPresetStore::findPreset(savedId));
    QTest::mouseClick(presets, Qt::LeftButton);
    QTRY_VERIFY(presets->view()->isVisible());
    QTest::keyClick(presets->view(), Qt::Key_End);
    QTest::keyClick(presets->view(), Qt::Key_Return);
    QCOMPARE(presets->currentData().toString(), savedId);

    choosePresetAction(SpeakerMixDialog::tr("Delete"), QMessageBox::No);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(SpeakerMixPresetStore::findPreset(savedId));
    QCOMPARE(presets->currentData().toString(), savedId);
    choosePresetAction(SpeakerMixDialog::tr("Delete"), QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!SpeakerMixPresetStore::findPreset(savedId));
    QCOMPARE(presets->currentIndex(), 0);
    QCOMPARE(savedPresets(), originalPresets);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
