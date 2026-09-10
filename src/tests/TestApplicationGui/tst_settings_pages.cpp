#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/Options/Pages/AutomationPage.h"
#include "UI/Dialogs/Options/Pages/InferencePage.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/PathListWidget.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/History/HistoryManager.h>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    void openOptionsPage(AppOptionsDialog &panel, AppOptionsGlobal::Option option) {
        panel.resize(920, 720);
        panel.show();
        panel.activateWindow();
        QTRY_VERIFY(panel.isVisible());
        QTRY_VERIFY(panel.isActiveWindow());
        auto *tabs = panel.findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(tabs);
        auto *item = tabs->item(static_cast<int>(option) - 1);
        QVERIFY(item);
        QTest::mouseClick(tabs->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tabs->visualItemRect(item).center());
        QTRY_COMPARE(tabs->currentItem(), item);
    }

    void replaceText(QLineEdit *editor, const QString &text) {
        QTest::mouseClick(editor, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QApplication::clipboard()->setText(text);
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keySequence(editor, QKeySequence::Paste);
        QCOMPARE(editor->text(), text);
    }

    void editAccessRoot(PathEditor *paths, const QString &path) {
        auto *list = paths->listWidget();
        QVERIFY(list->count() > 0);
        list->scrollToItem(list->item(0));
        QCoreApplication::processEvents();
        list->window()->activateWindow();
        QTRY_VERIFY(list->window()->isActiveWindow());
        const auto position = list->visualItemRect(list->item(0)).center();
        QVERIFY(list->viewport()->rect().contains(position));
        QSignalSpy doubleClicked(list, &QAbstractItemView::doubleClicked);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QVERIFY2(!doubleClicked.isEmpty(), qPrintable(path));
        QLineEdit *editor = nullptr;
        QTRY_VERIFY2(
            (editor = qobject_cast<QLineEdit *>(QApplication::focusWidget())) &&
                list->isAncestorOf(editor),
            qPrintable(QStringLiteral("Editing %1; focus widget: %2")
                           .arg(path, QApplication::focusWidget()
                                          ? QApplication::focusWidget()->metaObject()->className()
                                          : "none")));
        replaceText(editor, path);
        if (QTest::currentTestFailed())
            return;
        QSignalSpy changed(paths, &PathEditor::pathsChanged);
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_VERIFY(!changed.isEmpty());
    }

    void readSavedOptions(QJsonObject &options) {
        QFile config(appOptions->configPath());
        QVERIFY(config.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto saved = QJsonDocument::fromJson(config.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(saved.isObject());
        options = saved.object();
    }
}

void ApplicationGuiTests::appearanceInputsPersistAcrossReopening() {
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().appearance;
    const auto restore = qScopeGuard([&] { runtime.settings().updateAppearance({}, original); });
    const bool enabled = !original.animationEnabled;
    const double scale = original.animationTimeScale == 1.75 ? 0.75 : 1.75;

    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Appearance);
        if (QTest::currentTestFailed())
            return;
        auto *animation = panel.findChild<SwitchButton *>("appearanceAnimationEnabled");
        auto *duration = panel.findChild<QLineEdit *>("appearanceAnimationTimeScale");
        QVERIFY(animation);
        QVERIFY(duration);
        QTRY_VERIFY(animation->isVisible());
        QTRY_VERIFY(duration->isVisible());
        QCOMPARE(animation->value(), original.animationEnabled);

        QTest::mouseClick(animation, Qt::LeftButton);
        QCOMPARE(animation->value(), enabled);
        QTest::mouseClick(duration, Qt::LeftButton);
        QTRY_VERIFY(duration->hasFocus());
        QApplication::clipboard()->setText(QLocale().toString(scale));
        QTest::keySequence(duration, QKeySequence::SelectAll);
        QTest::keySequence(duration, QKeySequence::Paste);
        QCOMPARE(QLocale().toDouble(duration->text()), scale);
        QVERIFY(duration->hasAcceptableInput());
        QSignalSpy committed(duration, &QLineEdit::editingFinished);
        QTest::keyClick(duration, Qt::Key_Tab);
        QCOMPARE(committed.size(), 1);

        const auto changed = runtime.settings().getSettings();
        QVERIFY(changed);
        QCOMPARE(changed.get().appearance.animationEnabled, enabled);
        QCOMPARE(changed.get().appearance.animationTimeScale, scale);
        QFile config(appOptions->configPath());
        QVERIFY(config.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto saved = QJsonDocument::fromJson(config.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const auto appearance = saved.object().value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("animationEnabled")).toBool(), enabled);
        QCOMPARE(appearance.value(QStringLiteral("animationTimeScale")).toDouble(), scale);
        panel.close();
    }

    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::Appearance);
    if (QTest::currentTestFailed())
        return;
    auto *animation = reopened.findChild<SwitchButton *>("appearanceAnimationEnabled");
    auto *duration = reopened.findChild<QLineEdit *>("appearanceAnimationTimeScale");
    QVERIFY(animation);
    QVERIFY(duration);
    QCOMPARE(animation->value(), enabled);
    QCOMPARE(QLocale().toDouble(duration->text()), scale);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::automationAccessInputsPersistAndRejectMissingFolders() {
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto original = *appOptions->automation();
    const auto restore = qScopeGuard([&] {
        *appOptions->automation() = original;
        appOptions->saveAndNotify(AppOptionsGlobal::Automation);
    });
    const auto initialRoot = dataRoot.filePath(QStringLiteral("initial-access"));
    const auto selectedRoot = dataRoot.filePath(QStringLiteral("selected-access"));
    const auto missingRoot = dataRoot.filePath(QStringLiteral("missing-access"));
    QVERIFY(QDir().mkpath(initialRoot));
    QVERIFY(QDir().mkpath(selectedRoot));
    appOptions->automation()->accessRoots = {initialRoot};
    appOptions->automation()->controlLevel = AutomationOption::ControlLevel::L1;
    QVERIFY(appOptions->saveAndNotify(AppOptionsGlobal::Automation));
    const auto canonicalInitialRoot = QFileInfo(initialRoot).canonicalFilePath();

    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Automation);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AutomationPage *>();
        auto *level = panel.findChild<ComboBox *>("automationControlLevel");
        auto *paths = panel.findChild<PathEditor *>("automationAccessRoots");
        QVERIFY(page);
        QVERIFY(level);
        QVERIFY(paths);
        page->ensureWidgetVisible(level);
        QTest::mouseClick(level, Qt::LeftButton);
        QTRY_VERIFY(level->view()->isVisible());
        QTest::keyClick(level->view(), Qt::Key_Home);
        QTest::keyClick(level->view(), Qt::Key_Down);
        QTest::keyClick(level->view(), Qt::Key_Down);
        QTest::keyClick(level->view(), Qt::Key_Return);
        QTRY_VERIFY(!level->view()->isVisible());
        QCOMPARE(appOptions->automation()->controlLevel, AutomationOption::ControlLevel::L3);
        QCOMPARE(level->currentData().toInt(),
                 static_cast<int>(AutomationOption::ControlLevel::L3));

        page->ensureWidgetVisible(paths);
        editAccessRoot(paths, missingRoot);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(appOptions->automation()->accessRoots, QStringList{canonicalInitialRoot});
        QCOMPARE(paths->paths(), QStringList{missingRoot});
        bool warningVisible = false;
        for (const auto *label : page->findChildren<QLabel *>())
            warningVisible |= label->isVisible() && label->text().contains(missingRoot);
        QVERIFY(warningVisible);

        editAccessRoot(paths, selectedRoot);
        if (QTest::currentTestFailed())
            return;
        const auto canonicalRoot = QFileInfo(selectedRoot).canonicalFilePath();
        QCOMPARE(appOptions->automation()->accessRoots, QStringList{canonicalRoot});
        QCOMPARE(paths->paths(), QStringList{canonicalRoot});
        QJsonObject saved;
        readSavedOptions(saved);
        if (QTest::currentTestFailed())
            return;
        const auto automation = saved.value(QStringLiteral("automation")).toObject();
        QCOMPARE(automation.value(QStringLiteral("controlLevel")).toString(), QStringLiteral("l3"));
        QCOMPARE(automation.value(QStringLiteral("accessRoots")).toArray(),
                 QJsonArray{canonicalRoot});
        panel.close();
    }

    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::Automation);
    if (QTest::currentTestFailed())
        return;
    const auto *level = reopened.findChild<ComboBox *>("automationControlLevel");
    const auto *paths = reopened.findChild<PathEditor *>("automationAccessRoots");
    QVERIFY(level);
    QVERIFY(paths);
    QCOMPARE(level->currentData().toInt(), static_cast<int>(AutomationOption::ControlLevel::L3));
    QCOMPARE(paths->paths(), QStringList{QFileInfo(selectedRoot).canonicalFilePath()});
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::inferenceInputsPersistAcrossReopening() {
    QTRY_COMPARE(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready);
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().inference;
    const auto restore = qScopeGuard([&] { runtime.settings().updateInference({}, original); });
    const auto steps = original.samplingSteps == 37 ? 23 : 37;
    const auto autoStart = !original.autoStartInference;

    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Inference);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<InferencePage *>();
        auto *sampling = panel.findChild<ComboBox *>("inferenceSamplingSteps");
        auto *automatic = panel.findChild<SwitchButton *>("inferenceAutoStart");
        QVERIFY(page);
        QVERIFY(sampling);
        QVERIFY(automatic);
        QCOMPARE(appOptions->inference()->executionProvider, QStringLiteral("CPU"));
        page->ensureWidgetVisible(sampling);
        replaceText(sampling->lineEdit(), QLocale().toString(steps));
        if (QTest::currentTestFailed())
            return;
        QTest::keyClick(sampling->lineEdit(), Qt::Key_Tab);
        page->ensureWidgetVisible(automatic);
        QTest::mouseClick(automatic, Qt::LeftButton);
        QCOMPARE(automatic->value(), autoStart);

        const auto changed = runtime.settings().getSettings();
        QVERIFY(changed);
        QCOMPARE(changed.get().inference.samplingSteps, steps);
        QCOMPARE(changed.get().inference.autoStartInference, autoStart);
        QJsonObject saved;
        readSavedOptions(saved);
        if (QTest::currentTestFailed())
            return;
        const auto inference = saved.value(QStringLiteral("inference")).toObject();
        QCOMPARE(inference.value(QStringLiteral("samplingSteps")).toInt(), steps);
        QCOMPARE(inference.value(QStringLiteral("autoStartInfer")).toBool(), autoStart);
        panel.close();
    }

    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::Inference);
    if (QTest::currentTestFailed())
        return;
    const auto *sampling = reopened.findChild<ComboBox *>("inferenceSamplingSteps");
    const auto *automatic = reopened.findChild<SwitchButton *>("inferenceAutoStart");
    QVERIFY(sampling);
    QVERIFY(automatic);
    QCOMPARE(QLocale().toInt(sampling->currentText()), steps);
    QCOMPARE(automatic->value(), autoStart);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::cacheCleanupRequiresConfirmationAndRefreshesThePage() {
    QTRY_COMPARE(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    const auto originalDirectory = appOptions->inference()->cacheDirectory;
    const auto restore =
        qScopeGuard([&] { appOptions->inference()->cacheDirectory = originalDirectory; });
    appOptions->inference()->cacheDirectory = cacheDirectory.path();
    const auto cachePath = cacheDirectory.filePath(
        QStringLiteral("infer-duration-output-%1.json").arg(QString(40, QLatin1Char('a'))));
    const auto otherPath = cacheDirectory.filePath(QStringLiteral("user-note.txt"));
    for (const auto &path : {cachePath, otherPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("test data"), qint64(9));
    }

    AppOptionsDialog panel;
    openOptionsPage(panel, AppOptionsGlobal::Inference);
    if (QTest::currentTestFailed())
        return;
    auto *page = panel.findChild<InferencePage *>();
    auto *clean = panel.findChild<Button *>("inferenceCleanCache");
    auto *refresh = panel.findChild<Button *>("inferenceScanCache");
    auto *stats = panel.findChild<QLabel *>("inferenceCacheStats");
    QVERIFY(page);
    QVERIFY(clean);
    QVERIFY(refresh);
    QVERIFY(stats);
    QTRY_VERIFY(refresh->isEnabled());
    QTRY_VERIFY(clean->isEnabled());
    const auto scannedText = stats->text();
    QVERIFY(!scannedText.isEmpty());
    page->ensureWidgetVisible(clean);

    const auto respondToConfirmation = [&](const bool accept) {
        bool responded = false;
        QTimer respond;
        respond.setSingleShot(true);
        connect(&respond, &QTimer::timeout, &panel, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *dialog = qobject_cast<MessageDialog *>(modal);
            if (!dialog && modal)
                modal->reject();
            QVERIFY(dialog);
            const auto closeOnFailure = qScopeGuard([&] {
                if (!responded)
                    dialog->reject();
            });
            const auto label = InferencePage::tr(accept ? "Clean Up" : "Cancel");
            for (auto *button : dialog->findChildren<Button *>()) {
                if (button->text() == label) {
                    QTest::mouseClick(button, Qt::LeftButton);
                    responded = true;
                    break;
                }
            }
            QVERIFY(responded);
        });
        respond.start(0);
        QTest::mouseClick(clean, Qt::LeftButton);
        respond.stop();
        QVERIFY(responded);
    };

    respondToConfirmation(false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(QFileInfo::exists(cachePath));
    QVERIFY(QFileInfo::exists(otherPath));
    QCOMPARE(stats->text(), scannedText);
    QVERIFY(clean->isEnabled());

    respondToConfirmation(true);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(!QFileInfo::exists(cachePath));
    QTRY_VERIFY(refresh->isEnabled());
    QTRY_COMPARE(stats->text(), InferencePage::tr("No cache files"));
    QVERIFY(!clean->isEnabled());
    QVERIFY(QFileInfo::exists(otherPath));
    QVERIFY(!historyManager->canUndo());
}
