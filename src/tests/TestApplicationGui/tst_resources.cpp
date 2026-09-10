#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/PackageManager/PackageManagerDialog.h"
#include "UI/Dialogs/PackageManager/PackageDetailsHeader.h"
#include "UI/Dialogs/ResourceCheck/AudioResourcePage.h"
#include "UI/Dialogs/ResourceCheck/ResourceCheckDialog.h"
#include "Utils/UiLanguageManager.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QScopeGuard>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest/QTest>

namespace {
    void enterResourceText(QLineEdit *editor, const QString &text) {
        QVERIFY(editor);
        QTest::mouseClick(editor, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QTest::keySequence(editor, QKeySequence::SelectAll);
        if (text.isEmpty()) {
            QTest::keyClick(editor, Qt::Key_Backspace);
        } else {
            QApplication::clipboard()->setText(text);
            QTest::keySequence(editor, QKeySequence::Paste);
        }
        QCOMPARE(editor->text(), text);
    }

    QPushButton *resourceButton(QWidget *parent, const QString &text) {
        for (auto *button : parent->findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }
}

void ApplicationGuiTests::packageSearchShowsTheSelectedPackageDetails() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready,
                              15000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              15000);
    PackageInfo selectedPackage;
    const auto packages = packageManager->installedPackages().successfulPackages;
    for (const auto &package : packages) {
        for (const auto &singer : package.singers()) {
            if (singer.singerId() == TestSupport::fixtureSingerId())
                selectedPackage = package;
        }
    }
    QVERIFY(!selectedPackage.isEmpty());
    const auto before = context->m_coreRuntime->documentVersion();

    PackageManagerDialog dialog;
    dialog.show();
    dialog.activateWindow();
    auto *search = dialog.findChild<QLineEdit *>();
    auto *list =
        dialog.findChild<QListView *>(QStringLiteral("PackageManagerDialogPackageListView"));
    auto *placeholder = dialog.findChild<QLabel *>(QStringLiteral("lbPackageDetailsPlaceholder"));
    auto *details =
        dialog.findChild<QWidget *>(QStringLiteral("PackageManagerDialogDetailsWidget"));
    auto *packageId = dialog.findChild<QLabel *>(QStringLiteral("lbPackageId"));
    auto *vendor = dialog.findChild<QLabel *>(QStringLiteral("lbVendor"));
    auto *version = dialog.findChild<QLabel *>(QStringLiteral("lbVersion"));
    auto *verify = resourceButton(&dialog, PackageDetailsHeader::tr("Verify"));
    QVERIFY(list);
    QVERIFY(list->model());
    QVERIFY(placeholder);
    QVERIFY(details);
    QVERIFY(packageId);
    QVERIFY(vendor);
    QVERIFY(version);
    QVERIFY(verify);
    QTRY_VERIFY(placeholder->isVisible());

    enterResourceText(search, selectedPackage.id().toUpper());
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(list->model()->rowCount(), 1);
    const auto index = list->model()->index(0, 0);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(index).center());
    QTRY_VERIFY(details->isVisible());
    QVERIFY(!placeholder->isVisible());
    QCOMPARE(packageId->text(), selectedPackage.id());
    QCOMPARE(vendor->text(),
             selectedPackage.displayVendor(UiLanguageManager::currentBcp47Candidates()));
    QCOMPARE(version->text(), QStringLiteral("v") + selectedPackage.version().toString());
    QVERIFY(verify->isEnabled());
    QVERIFY(verify->isVisible());

    enterResourceText(search, QStringLiteral("no-such-package-for-resource-search"));
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(list->model()->rowCount(), 0);
    QTRY_VERIFY(placeholder->isVisible());
    QVERIFY(!details->isVisible());
    enterResourceText(search, {});
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(list->model()->rowCount() > 0);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::missingAudioResourceRelinkCanBeCanceledAndCommitted() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto missingPath = directory.filePath(QStringLiteral("gone/original.wav"));
    const auto replacementPath = directory.filePath(QStringLiteral("replacement.wav"));
    const auto error = createWaveFixture(replacementPath);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    auto &runtime = *context->m_coreRuntime;
    const auto releaseAudio = qScopeGuard([&] {
        const auto reset = runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        QTest::qVerify(bool(reset), "document reset", "release the temporary audio document",
                       __FILE__, __LINE__);
        const auto finished = QTest::qWaitFor([] { return taskManager->tasks().isEmpty(); }, 10000);
        QTest::qVerify(finished, "audio tasks finished", "release temporary audio files", __FILE__,
                       __LINE__);
        if (QTest::currentTestFailed()) {
            directory.setAutoRemove(false);
            qWarning() << "Audio resource fixture retained at" << directory.path();
        }
    });
    auto draft = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Audio");
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Audio;
    clip.properties.name = QStringLiteral("Missing audio");
    clip.properties.length = 480;
    clip.properties.clipLen = 480;
    clip.audioPath = missingPath;
    track.clips.append(clip);
    draft.tracks.append(track);
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), draft));
    QCOMPARE(appModel->tracks().size(), 1);
    QCOMPARE(appModel->tracks().first()->clips().count(), 1);
    auto *audio = qobject_cast<AudioClip *>(*appModel->tracks().first()->clips().begin());
    QVERIFY(audio);
    QTRY_COMPARE(audio->pathStatus(), AudioClip::PathStatus::Missing);
    historyManager->reset();
    const auto before = runtime.documentVersion();

    const auto nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard(
        [&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled); });
    ResourceCheckDialog dialog;
    auto *page = new AudioResourcePage({audio->id()}, {}, &dialog);
    dialog.addPage(page);
    dialog.finalizePages();
    dialog.show();
    dialog.activateWindow();
    auto *tree = page->findChild<QTreeWidget *>();
    auto *relink = resourceButton(page, AudioResourcePage::tr("Relink..."));
    auto *confirm = resourceButton(page, AudioResourcePage::tr("Confirm"));
    QVERIFY(tree);
    QVERIFY(relink);
    QVERIFY(confirm);
    QCOMPARE(tree->topLevelItemCount(), 1);
    auto *row = tree->topLevelItem(0);
    QCOMPARE(row->text(2), missingPath);
    QCOMPARE(row->text(3), AudioResourcePage::tr("Missing"));
    QVERIFY(page->hasPendingIssues());
    QTRY_VERIFY(tree->isVisible());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(row).center());
    QVERIFY(relink->isEnabled());
    QVERIFY(!confirm->isEnabled());

    const auto chooseReplacement = [&](const bool accept) {
        bool interacted = false;
        QElapsedTimer waitingForPicker;
        QTimer chooseFile;
        chooseFile.setInterval(10);
        connect(&chooseFile, &QTimer::timeout, &dialog, [&] {
            auto *active = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            bool owned = false;
            for (const QObject *ancestor = active; ancestor; ancestor = ancestor->parent()) {
                if (ancestor == &dialog) {
                    owned = true;
                    break;
                }
            }
            auto *picker = owned ? qobject_cast<QFileDialog *>(active) : nullptr;
            if (!picker) {
                if (waitingForPicker.hasExpired(5000)) {
                    chooseFile.stop();
                    if (owned)
                        active->reject();
                    QFAIL("The audio resource file picker did not become active");
                }
                return;
            }
            chooseFile.stop();
            const auto closeOnFailure = qScopeGuard([&] {
                if (QTest::currentTestFailed())
                    picker->reject();
            });
            auto *name = picker->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
            enterResourceText(name, QDir::toNativeSeparators(replacementPath));
            if (QTest::currentTestFailed())
                return;
            if (accept) {
                auto *buttons = picker->findChild<QDialogButtonBox *>();
                QVERIFY(buttons);
                auto *open = buttons->button(QDialogButtonBox::Open);
                QVERIFY(open);
                QVERIFY(open->isEnabled());
                QTest::mouseClick(open, Qt::LeftButton);
            } else {
                QTest::keyClick(name, Qt::Key_Escape);
            }
            QTRY_VERIFY(!picker->isVisible());
            interacted = true;
        });
        waitingForPicker.start();
        chooseFile.start();
        QTest::mouseClick(relink, Qt::LeftButton);
        chooseFile.stop();
        QVERIFY(interacted);
    };

    chooseReplacement(false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(audio->path(), missingPath);
    QCOMPARE(audio->pathStatus(), AudioClip::PathStatus::Missing);
    QCOMPARE(row->text(3), AudioResourcePage::tr("Missing"));
    QVERIFY(page->hasPendingIssues());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    chooseReplacement(true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(QDir::cleanPath(audio->path()), QDir::cleanPath(replacementPath));
    QTRY_COMPARE(audio->audioInfo().frames, 800);
    QCOMPARE(audio->audioInfo().sampleRate, 8000);
    QCOMPARE(audio->audioInfo().channels, 1);
    QVERIFY(!audio->audioInfo().peakCache.isEmpty());
    QCOMPARE(audio->pathStatus(), AudioClip::PathStatus::Normal);
    QCOMPARE(QDir::cleanPath(row->text(2)), QDir::cleanPath(replacementPath));
    QCOMPARE(row->text(3), AudioResourcePage::tr("Resolved"));
    QVERIFY(!page->hasPendingIssues());
    QVERIFY(!relink->isEnabled());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    auto *close = resourceButton(&dialog, ResourceCheckDialog::tr("Close"));
    QVERIFY(close);
    QTest::mouseClick(close, Qt::LeftButton);
    QVERIFY(!dialog.isVisible());

    historyManager->undo();
    QCOMPARE(audio->path(), missingPath);
    QTRY_COMPARE(audio->pathStatus(), AudioClip::PathStatus::Missing);
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QTRY_COMPARE(audio->pathStatus(), AudioClip::PathStatus::Normal);
    QTRY_COMPARE(audio->audioInfo().frames, 800);
    QCOMPARE(QDir::cleanPath(audio->path()), QDir::cleanPath(replacementPath));
}
