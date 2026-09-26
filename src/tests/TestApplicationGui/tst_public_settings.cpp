#include "tst_application_gui.h"
#include "../TestSupport/MainWindowFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "Utils/UiLanguageManager.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QScopeGuard>
#include <QtTest/QTest>

void ApplicationGuiTests::publicUiSettingsPersistAndRollback_data() {
    QTest::addColumn<bool>("theme");
    QTest::newRow("theme") << true;
    QTest::newRow("ui-language") << false;
}

void ApplicationGuiTests::publicUiSettingsPersistAndRollback() {
    QFETCH(bool, theme);
    auto &runtime = *context->m_coreRuntime;
    const auto original = runtime.settings().getSettings();
    QVERIFY(original);
    UiLanguageManager languageManager;
    auto *themes = ThemeManager::instance();
    const auto restoreSettings = qScopeGuard([&] {
        QVERIFY(runtime.settings().updateTheme({}, {.themeId = original.get().appearance.themeId}));
        QVERIFY(runtime.settings().updateUiLanguage(
            {}, {.uiLanguage = original.get().general.uiLanguage}));
    });
    QVERIFY(runtime.settings().updateTheme({}, {.themeId = ThemeIds::darkThemePreferenceId()}));
    QVERIFY(runtime.settings().updateUiLanguage({}, {.uiLanguage = UiLanguageManager::English}));
    TestSupport::MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(runtime.project().renameTrack(
        commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
        QStringLiteral("Unsaved during UI settings")));
    const auto version = runtime.documentVersion();
    const auto model = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undo = historyManager->nextUndoEntry();
    const auto before = runtime.settings().getSettings();
    QVERIFY(before);
    const auto initialStyle = host.window->styleSheet();
    auto *timeline = host.window->findChild<QWidget *>("pianoRollTimelineView");
    QVERIFY(timeline);
    const auto initialTimelineColor = timeline->palette().color(QPalette::Window);
    auto *menuBar = host.window->findChild<MainMenuView *>();
    QVERIFY(menuBar);
    QAction *undoAction = nullptr;
    for (auto *entry : menuBar->actions()) {
        if (auto *menu = entry->menu()) {
            for (auto *action : menu->actions()) {
                if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Z")))
                    undoAction = action;
            }
        }
    }
    QVERIFY(undoAction && undoAction->isEnabled());
    const auto initialUndoText = undoAction->text();
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard files;
    Automation::AdmissionController admission;
    Automation::PublicAutomationRegistry registry(runtime, access, files, admission);
    const auto method = theme ? QStringLiteral("settings.theme.update")
                              : QStringLiteral("settings.ui_language.update");
    const auto field = theme ? QStringLiteral("theme_id") : QStringLiteral("ui_language");
    const auto target =
        theme ? ThemeIds::lightThemePreferenceId() : UiLanguageManager::SimplifiedChinese;
    const auto apply = [&] {
        return registry.invoke(method,
                               {
                                   {field, target}
        },
                               {.clientId = QStringLiteral("ui-settings-client"),
                                .source = Automation::InvocationSource::PublicJsonRpc});
    };
    const auto config = appOptions->configPath();
    const auto backup = config + QStringLiteral(".ui-settings-backup");
    QVERIFY(QFile::rename(config, backup));
    const auto restoreFile = qScopeGuard([&] {
        if (QFile::exists(backup)) {
            if (QFileInfo(config).isDir())
                QVERIFY(QDir().rmdir(config));
            QVERIFY(QFile::rename(backup, config));
        }
    });
    QVERIFY(QDir().mkdir(config));
    const auto failed = apply();
    QVERIFY(!failed);
    QCOMPARE(failed.getError().fieldPath, field);
    const auto afterFailure = runtime.settings().getSettings();
    QVERIFY(afterFailure);
    QCOMPARE(afterFailure.get().general, before.get().general);
    QCOMPARE(afterFailure.get().appearance, before.get().appearance);
    QTRY_COMPARE(themes->currentThemeId(),
                 ThemeIds::themeIdForPreference(ThemeIds::darkThemePreferenceId()));
    QTRY_COMPARE(languageManager.effectiveLanguageId(), UiLanguageManager::English);
    QCOMPARE(host.window->styleSheet(), initialStyle);
    QCOMPARE(timeline->palette().color(QPalette::Window), initialTimelineColor);
    QCOMPARE(undoAction->text(), initialUndoText);
    QCOMPARE(runtime.documentVersion(), version);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
    QCOMPARE(historyManager->nextUndoEntry(), undo);

    QVERIFY(QDir().rmdir(config));
    QVERIFY(QFile::rename(backup, config));
    const auto applied = apply();
    QVERIFY2(applied, qPrintable(applied ? QString{} : applied.getError().message));
    AppOptions stored;
    if (theme) {
        QCOMPARE(stored.appearance()->themeId, target);
        QTRY_COMPARE(themes->currentThemeId(), ThemeIds::themeIdForPreference(target));
        QTRY_COMPARE(host.window->styleSheet(), themes->styleSheet());
        QTRY_COMPARE(timeline->palette().color(QPalette::Window).rgba(),
                     themes->semanticColor(QStringLiteral("timeline.background")).rgba());
        QVERIFY(host.window->styleSheet() != initialStyle);
    } else {
        QCOMPARE(stored.general()->uiLanguage, target);
        QTRY_COMPARE(languageManager.effectiveLanguageId(), target);
        QTRY_VERIFY(undoAction->text().startsWith(
            QCoreApplication::translate("MainMenuViewPrivate", "&Undo")));
        QVERIFY(undoAction->text() != initialUndoText);
        QCOMPARE(undoAction->shortcut(), QKeySequence(QStringLiteral("Ctrl+Z")));
    }
    QCOMPARE(runtime.documentVersion(), version);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    QVERIFY(!historyManager->isOnSavePoint());
}
