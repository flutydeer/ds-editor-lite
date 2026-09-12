#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/EditorAutomationRuntimeStatus.h"
#include "Automation/Mcp/EditorMcpController.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/Options/Pages/AutomationPage.h"
#include "UI/Dialogs/Options/Pages/InferencePage.h"
#include "UI/Dialogs/Options/Pages/GeneralPage.h"
#include "UI/Dialogs/Options/Pages/AppearancePage.h"
#include "UI/Dialogs/Options/Pages/DeveloperPage.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "UI/Window/MainWindow.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "Utils/UiLanguageManager.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/PathListWidget.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Controls/FileSelector.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/AutomationWire/McpProtocol.h>

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
#include <QMenu>
#include <QLocale>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QSpinBox>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>
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
        const auto visibleRow =
            list->visualItemRect(list->item(0)).intersected(list->viewport()->rect());
        QVERIFY2(!visibleRow.isEmpty(), qPrintable(path));
        const auto position = visibleRow.center();
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

    void clickOption(AutomationPage *page, QWidget *control) {
        QVERIFY(control);
        page->ensureWidgetVisible(control);
        QTRY_VERIFY(control->isVisible());
        QVERIFY(control->isEnabled());
        QTest::mouseClick(control, Qt::LeftButton);
    }

    void selectComboIndex(ComboBox *combo, int index) {
        QVERIFY(combo);
        QVERIFY(index >= 0 && index < combo->count());
        QTest::mouseClick(combo, Qt::LeftButton);
        QTRY_VERIFY(combo->view()->isVisible());
        QTest::keyClick(combo->view(), Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(combo->view(), Qt::Key_Down);
        QTest::keyClick(combo->view(), Qt::Key_Return);
        QCOMPARE(combo->currentIndex(), index);
        QTRY_VERIFY(!combo->view()->isVisible());
        combo->window()->activateWindow();
        QTRY_VERIFY(combo->window()->isActiveWindow());
    }
}

void ApplicationGuiTests::experimentalRendererSettingPersistsWhenRestartIsDeferred() {
    auto &runtime = *context->m_coreRuntime;
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto restore = qScopeGuard(
        [&] { QVERIFY(runtime.settings().updateDeveloper({}, settings.get().developer)); });
    auto initial = settings.get().developer;
    initial.editorRenderBackend = Automation::EditorRenderBackend::Legacy;
    QVERIFY(runtime.settings().updateDeveloper({}, initial));
    const auto before = runtime.documentVersion();
    const auto experimental =
        static_cast<int>(DeveloperOption::EditorRenderBackend::RhiExperimental);
    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::DeveloperOptions);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<DeveloperPage *>();
        QVERIFY(page);
        auto *backend = page->findChild<ComboBox *>();
        QVERIFY(backend);
        page->ensureWidgetVisible(backend);
        const auto index = backend->findData(experimental);
        QVERIFY(index >= 0);
        QTest::mouseClick(backend, Qt::LeftButton);
        QTRY_VERIFY(backend->view()->isVisible());
        QTest::keyClick(backend->view(), Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(backend->view(), Qt::Key_Down);
        QTest::keyClick(backend->view(), Qt::Key_Return);
        QPointer<RestartDialog> prompt;
        QTRY_VERIFY((prompt = page->findChild<RestartDialog *>()) && prompt->isVisible());
        const auto closePrompt = qScopeGuard([&] {
            if (prompt)
                prompt->reject();
        });
        Button *later = nullptr;
        for (auto *button : prompt->findChildren<Button *>()) {
            if (button->text() == RestartDialog::tr("Restart Later"))
                later = button;
        }
        QVERIFY(later);
        QSignalSpy rejected(prompt, &QDialog::rejected);
        QTest::mouseClick(later, Qt::LeftButton);
        QCOMPARE(rejected.size(), 1);
        QVERIFY(!prompt || !prompt->isVisible());
        QCOMPARE(static_cast<int>(appOptions->developer()->editorRenderBackend), experimental);
        panel.close();
    }
    AppOptions persisted;
    QCOMPARE(static_cast<int>(persisted.developer()->editorRenderBackend), experimental);
    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::DeveloperOptions);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<DeveloperPage *>();
    QVERIFY(page);
    auto *backend = page->findChild<ComboBox *>();
    QVERIFY(backend);
    QCOMPARE(backend->currentData().toInt(), experimental);
    QVERIFY(!reopened.findChild<RestartDialog *>());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::generalSettingsKeepSeparateDefaultLyricsForEachLanguage() {
    auto &runtime = *context->m_coreRuntime;
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto restore =
        qScopeGuard([&] { QVERIFY(runtime.settings().updateGeneral({}, settings.get().general)); });
    const auto before = runtime.documentVersion();
    const auto lyricEditor = [](GeneralPage *page) {
        for (auto *editor : page->findChildren<LineEdit *>()) {
            if (!qobject_cast<FileSelector *>(editor->parentWidget()))
                return editor;
        }
        return static_cast<LineEdit *>(nullptr);
    };
    const auto chooseLanguage = [](GeneralPage *page, LanguageComboBox *combo,
                                   const QString &language) {
        const auto index = combo->findData(language);
        QVERIFY(index >= 0);
        page->ensureWidgetVisible(combo);
        QTest::mouseClick(combo, Qt::LeftButton);
        QTRY_VERIFY(combo->view()->isVisible());
        QTest::keyClick(combo->view(), Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(combo->view(), Qt::Key_Down);
        QTest::keyClick(combo->view(), Qt::Key_Return);
        QCOMPARE(combo->currentLanguage(), language);
    };
    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::General);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<GeneralPage *>();
        QVERIFY(page);
        auto *language = page->findChild<LanguageComboBox *>();
        auto *lyric = lyricEditor(page);
        QVERIFY(language && lyric);
        chooseLanguage(page, language, QStringLiteral("eng"));
        if (QTest::currentTestFailed())
            return;
        replaceText(lyric, QStringLiteral("doo"));
        QTest::keyClick(lyric, Qt::Key_Return);
        chooseLanguage(page, language, QStringLiteral("cmn"));
        if (QTest::currentTestFailed())
            return;
        replaceText(lyric, QStringLiteral("啦"));
        QTest::keyClick(lyric, Qt::Key_Return);
        chooseLanguage(page, language, QStringLiteral("eng"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(lyric->text(), QStringLiteral("doo"));
        QCOMPARE(appOptions->general()->defaultLyrics.value(QStringLiteral("cmn")),
                 QStringLiteral("啦"));
        panel.close();
    }
    AppOptions reopenedOptions;
    QCOMPARE(reopenedOptions.general()->defaultLyrics.value(QStringLiteral("eng")),
             QStringLiteral("doo"));
    QCOMPARE(reopenedOptions.general()->defaultLyrics.value(QStringLiteral("cmn")),
             QStringLiteral("啦"));
    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::General);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<GeneralPage *>();
    QVERIFY(page);
    auto *language = page->findChild<LanguageComboBox *>();
    auto *lyric = lyricEditor(page);
    QVERIFY(language && lyric);
    QCOMPARE(lyric->text(), QStringLiteral("doo"));
    chooseLanguage(page, language, QStringLiteral("cmn"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(lyric->text(), QStringLiteral("啦"));
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::switchingUiLanguagePreservesSettingsAndTheOpenDocument() {
    auto &runtime = *context->m_coreRuntime;
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto restore =
        qScopeGuard([&] { QVERIFY(runtime.settings().updateGeneral({}, settings.get().general)); });
    auto general = settings.get().general;
    general.uiLanguage = UiLanguageManager::English;
    general.defaultLyrics[general.defaultSingingLanguage] = QStringLiteral("retained lyric");
    QVERIFY(runtime.settings().updateGeneral({}, general));
    UiLanguageManager languageManager;
    languageManager.setPreference(general.uiLanguage);
    MainWindow window;
    window.resize(1200, 800);
    auto *menuBar = window.findChild<MainMenuView *>();
    QVERIFY(menuBar);
    menuBar->setNativeMenuBar(false);
    window.show();
    QTRY_VERIFY(window.isActiveWindow());
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QVERIFY(runtime.project().renameTrack(
        commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
        QStringLiteral("Retained track name")));
    const auto before = runtime.documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto selected = appStatus->selectedNotes.get();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    QVERIFY(beforeUndo);
    QAction *undo = nullptr;
    for (auto *entry : menuBar->actions()) {
        if (auto *menu = entry->menu()) {
            for (auto *action : menu->actions()) {
                if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Z")))
                    undo = action;
            }
        }
    }
    QVERIFY(undo && undo->isEnabled());
    const auto englishUndo = undo->text();

    AppOptionsDialog panel;
    for (const auto option : {AppOptionsGlobal::Audio, AppOptionsGlobal::Midi,
                              AppOptionsGlobal::Inference, AppOptionsGlobal::General}) {
        openOptionsPage(panel, option);
        if (QTest::currentTestFailed())
            return;
    }
    auto *page = panel.findChild<GeneralPage *>();
    auto *tabs = panel.findChild<QListWidget *>("AppOptionsDialogTabListWidget");
    QVERIFY(page && tabs);
    const auto selectedPage = tabs->currentRow();
    for (const auto &preference :
         {UiLanguageManager::SimplifiedChinese, UiLanguageManager::English}) {
        ComboBox *language = nullptr;
        for (auto *combo : page->findChildren<ComboBox *>()) {
            if (combo->findData(UiLanguageManager::English) >= 0 &&
                combo->findData(UiLanguageManager::SimplifiedChinese) >= 0)
                language = combo;
        }
        QVERIFY(language);
        const auto target = language->findData(preference);
        QPointer<QWidget> previousContent = page->widget();
        page->ensureWidgetVisible(language);
        QTest::mouseClick(language, Qt::LeftButton);
        QTRY_VERIFY(language->view()->isVisible());
        QTest::keyClick(language->view(), Qt::Key_Home);
        for (int index = 0; index < target; ++index)
            QTest::keyClick(language->view(), Qt::Key_Down);
        QTest::keyClick(language->view(), Qt::Key_Return);
        QTRY_COMPARE(languageManager.effectiveLanguageId(), preference);
        QTRY_VERIFY(previousContent.isNull());
        QCOMPARE(tabs->currentRow(), selectedPage);
        QCOMPARE(tabs->currentItem()->text(), AppOptionsDialog::tr("General"));
        QVERIFY(
            undo->text().startsWith(QCoreApplication::translate("MainMenuViewPrivate", "&Undo")));
        QCOMPARE(undo->shortcut(), QKeySequence(QStringLiteral("Ctrl+Z")));
        if (preference == UiLanguageManager::SimplifiedChinese)
            QVERIFY(undo->text() != englishUndo);
        else
            QCOMPARE(undo->text(), englishUndo);
        LineEdit *lyric = nullptr;
        for (auto *editor : page->findChildren<LineEdit *>()) {
            if (!qobject_cast<FileSelector *>(editor->parentWidget()))
                lyric = editor;
        }
        QVERIFY(lyric);
        QCOMPARE(lyric->text(), QStringLiteral("retained lyric"));
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QCOMPARE(appStatus->selectedNotes.get(), selected);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        AppOptions reopened;
        QCOMPARE(reopened.general()->uiLanguage, preference);
        QCOMPARE(reopened.general()->defaultLyricForLanguage(general.defaultSingingLanguage),
                 QStringLiteral("retained lyric"));
    }
}

void ApplicationGuiTests::appearanceInputsPersistAcrossReopening() {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    QTRY_VERIFY(window.isActiveWindow());
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto selectedNotes = appStatus->selectedNotes.get();
    const auto activeClip = appStatus->activeClipId.get();
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().appearance;
    const auto restore = qScopeGuard([&] {
        QVERIFY(runtime.settings().updateAppearance({}, original));
        QVERIFY(ThemeManager::instance()->applyThemePreference(original.themeId));
    });
    const bool enabled = !original.animationEnabled;
    const double scale = original.animationTimeScale == 1.75 ? 0.75 : 1.75;
    QString fontFamily;
    const auto controls = [](AppearancePage *page) {
        QPair<ComboBox *, ComboBox *> result;
        for (auto *combo : page->findChildren<ComboBox *>()) {
            if (combo->findData(ThemeIds::lightThemePreferenceId()) >= 0)
                result.first = combo;
            else
                result.second = combo;
        }
        return result;
    };

    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Appearance);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AppearancePage *>();
        QVERIFY(page);
        const auto [theme, font] = controls(page);
        QVERIFY(theme);
        QVERIFY(font);
        QString darkStyle;
        for (const auto &preference :
             {ThemeIds::darkThemePreferenceId(), ThemeIds::lightThemePreferenceId()}) {
            page->ensureWidgetVisible(theme);
            selectComboIndex(theme, theme->findData(preference));
            if (QTest::currentTestFailed())
                return;
            QCOMPARE(ThemeManager::instance()->currentThemeId(),
                     ThemeIds::themeIdForPreference(preference));
            QCOMPARE(window.styleSheet(), ThemeManager::instance()->styleSheet());
            QVERIFY(!window.styleSheet().isEmpty());
            if (preference == ThemeIds::darkThemePreferenceId())
                darkStyle = window.styleSheet();
            else
                QVERIFY(window.styleSheet() != darkStyle);
        }
        int fontIndex = -1;
        for (int index = 1; index < font->count(); ++index) {
            if (font->itemData(index).toString() != QApplication::font().family()) {
                fontIndex = index;
                break;
            }
        }
        QVERIFY(fontIndex > 0);
        fontFamily = font->itemData(fontIndex).toString();
        page->ensureWidgetVisible(font);
        selectComboIndex(font, fontIndex);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(QApplication::font().family(), fontFamily);
        auto *animation = panel.findChild<SwitchButton *>("appearanceAnimationEnabled");
        auto *duration = panel.findChild<QLineEdit *>("appearanceAnimationTimeScale");
        QVERIFY(animation);
        QVERIFY(duration);
        page->ensureWidgetVisible(animation);
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
        QCOMPARE(changed.get().appearance.themeId, ThemeIds::lightThemePreferenceId());
        QCOMPARE(changed.get().appearance.uiFontFamily, fontFamily);
        QFile config(appOptions->configPath());
        QVERIFY(config.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto saved = QJsonDocument::fromJson(config.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const auto appearance = saved.object().value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("animationEnabled")).toBool(), enabled);
        QCOMPARE(appearance.value(QStringLiteral("animationTimeScale")).toDouble(), scale);
        QCOMPARE(appearance.value(QStringLiteral("themeId")).toString(),
                 ThemeIds::lightThemePreferenceId());
        QCOMPARE(appearance.value(QStringLiteral("uiFontFamily")).toString(), fontFamily);
        panel.close();
    }

    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::Appearance);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<AppearancePage *>();
    QVERIFY(page);
    const auto [theme, font] = controls(page);
    QVERIFY(theme);
    QVERIFY(font);
    QCOMPARE(theme->currentData().toString(), ThemeIds::lightThemePreferenceId());
    QCOMPARE(font->currentData().toString(), fontFamily);
    auto *animation = reopened.findChild<SwitchButton *>("appearanceAnimationEnabled");
    auto *duration = reopened.findChild<QLineEdit *>("appearanceAnimationTimeScale");
    QVERIFY(animation);
    QVERIFY(duration);
    QCOMPARE(animation->value(), enabled);
    QCOMPARE(QLocale().toDouble(duration->text()), scale);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QCOMPARE(appStatus->selectedNotes.get(), selectedNotes);
    QCOMPARE(appStatus->activeClipId.get(), activeClip);
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

void ApplicationGuiTests::automationCustomToolsetInputsPersistAndExportPermissions() {
    const auto original = *appOptions->automation();
    const auto restore = qScopeGuard([&] {
        *appOptions->automation() = original;
        appOptions->saveAndNotify(AppOptionsGlobal::Automation);
    });
    auto *option = appOptions->automation();
    option->controlLevel = AutomationOption::ControlLevel::L1;
    option->customPermissions.clear();
    QVERIFY(appOptions->saveAndNotify(AppOptionsGlobal::Automation));
    const auto before = context->m_coreRuntime->documentVersion();
    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Automation);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AutomationPage *>();
        QVERIFY(page);
        auto *open = page->findChild<ToolButton *>("automationOpenToolsetButton");
        clickOption(page, open);
        if (QTest::currentTestFailed())
            return;
        auto *expand = page->findChild<ToolButton *>("automationCustomToolGroupExpand_tracks");
        auto *group = page->findChild<SwitchButton *>("automationCustomToolGroupSwitch_tracks");
        auto *rename = page->findChild<SwitchButton *>("automationCustomTool_tracks.rename");
        auto *list = page->findChild<SwitchButton *>("automationCustomTool_tracks.list");
        auto *back = page->findChild<ToolButton *>("automationCloseToolsetButton");
        QVERIFY(group && rename && list && back);
        QVERIFY(!group->value());
        QVERIFY(!rename->isVisible());
        clickOption(page, expand);
        clickOption(page, group);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(rename->value() && list->value());
        QVERIFY(option->customPermissionEnabled(QStringLiteral("tracks.rename")));
        clickOption(page, rename);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(group->value());
        QVERIFY(!option->customPermissionEnabled(QStringLiteral("tracks.rename")));
        QVERIFY(option->customPermissionEnabled(QStringLiteral("tracks.list")));

        clickOption(page, back);
        auto *importLevel = page->findChild<Button *>("automationImportControlLevelButton");
        clickOption(page, importLevel);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(rename->value());
        QVERIFY(option->customPermissionEnabled(QStringLiteral("tracks.rename")));
        clickOption(page, open);
        clickOption(page, group);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!rename->value() && !list->value());
        clickOption(page, rename);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(group->value());
        QVERIFY(!list->value());
        clickOption(page, expand);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!rename->isVisible());
        clickOption(page, back);
        auto *level = page->findChild<ComboBox *>("automationControlLevel");
        clickOption(page, level);
        if (QTest::currentTestFailed())
            return;
        QTest::keyClick(level->view(), Qt::Key_End);
        QTest::keyClick(level->view(), Qt::Key_Return);
        QTRY_VERIFY(!level->view()->isVisible());
        QCOMPARE(option->controlLevel, AutomationOption::ControlLevel::Custom);
        QVERIFY(!importLevel->isEnabled());

        clickOption(page, page->findChild<Button *>("automationStdioConfigurationCopyButton"));
        if (QTest::currentTestFailed())
            return;
        QJsonParseError error;
        const auto config =
            QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8(), &error).object();
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(QFileInfo(config.value(QStringLiteral("command")).toString()).isAbsolute());
        const auto args = config.value(QStringLiteral("args")).toArray();
        QVERIFY(args.contains(QStringLiteral("--include-tool=id:tracks.rename")));
        QVERIFY(!args.contains(QStringLiteral("--include-tool=id:tracks.list")));
        const auto levelFlag = args.toVariantList().indexOf(QStringLiteral("--control-level"));
        QVERIFY(levelFlag >= 0 && levelFlag + 1 < args.size());
        QCOMPARE(args.at(levelFlag + 1).toString(), QStringLiteral("l0"));
        QJsonObject saved;
        readSavedOptions(saved);
        if (QTest::currentTestFailed())
            return;
        AutomationOption loaded;
        loaded.load(saved.value(QStringLiteral("automation")).toObject());
        QCOMPARE(loaded.controlLevel, AutomationOption::ControlLevel::Custom);
        QVERIFY(loaded.customPermissionEnabled(QStringLiteral("tracks.rename")));
        QVERIFY(!loaded.customPermissionEnabled(QStringLiteral("tracks.list")));
    }
    AppOptionsDialog reopened;
    openOptionsPage(reopened, AppOptionsGlobal::Automation);
    if (QTest::currentTestFailed())
        return;
    auto *page = reopened.findChild<AutomationPage *>();
    QVERIFY(page);
    clickOption(page, page->findChild<ToolButton *>("automationOpenToolsetButton"));
    if (QTest::currentTestFailed())
        return;
    auto *rename = page->findChild<SwitchButton *>("automationCustomTool_tracks.rename");
    auto *list = page->findChild<SwitchButton *>("automationCustomTool_tracks.list");
    auto *group = page->findChild<SwitchButton *>("automationCustomToolGroupSwitch_tracks");
    QVERIFY(rename && list && group);
    QVERIFY(rename->value() && group->value());
    QVERIFY(!list->value());
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::automationServerReconfigurationUpdatesAccessAndConnectionDetails() {
    using namespace Automation::AutomationRuntimeStatus;
    namespace Mcp = AutomationWire::Mcp;
    const auto original = *appOptions->automation();
    const auto oldState = qApp->property(StateProperty);
    const auto oldEndpoint = qApp->property(EndpointProperty);
    const auto oldError = qApp->property(ErrorProperty);
    const auto restore = qScopeGuard([&] {
        qApp->setProperty(StateProperty, oldState);
        qApp->setProperty(EndpointProperty, oldEndpoint);
        qApp->setProperty(ErrorProperty, oldError);
        *appOptions->automation() = original;
        appOptions->saveAndNotify(AppOptionsGlobal::Automation);
    });
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer occupiedPort;
    QVERIFY(occupiedPort.listen(QHostAddress::LocalHost, 0));
    const auto port = occupiedPort.serverPort();
    auto *options = appOptions->automation();
    options->mcpEnabled = false;
    options->controlPort = port;
    options->accessRoots = {directory.path()};
    options->controlLevel = AutomationOption::ControlLevel::L1;
    options->customPermissions.clear();
    SingleInstanceCoordinator coordinator(directory.path(), QUuid::createUuid().toString());
    Automation::EditorMcpController controller(*context->m_coreRuntime, *appOptions, coordinator,
                                               AppHostMode::Gui, {});
    const auto before = context->m_coreRuntime->documentVersion();
    AppOptionsDialog panel;
    openOptionsPage(panel, AppOptionsGlobal::Automation);
    if (QTest::currentTestFailed())
        return;
    auto *page = panel.findChild<AutomationPage *>();
    QVERIFY(page);
    auto *endpointCopy = page->findChild<Button *>("automationStreamableHttpEndpointCopyButton");
    auto *configCopy = page->findChild<Button *>("automationStreamableHttpConfigurationCopyButton");
    auto *enabled = page->findChild<SwitchButton *>();
    auto *portEditor = page->findChild<QSpinBox *>();
    QVERIFY(enabled && portEditor);
    const auto configuredEndpoint = [&] {
        return QStringLiteral("http://127.0.0.1:%1/mcp").arg(options->controlPort);
    };
    clickOption(page, endpointCopy);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(QApplication::clipboard()->text(), configuredEndpoint());
    QCOMPARE(coordinator.automationState().state, SingleInstanceAutomationState::ServerDisabled);

    clickOption(page, enabled);
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(coordinator.automationState().state, SingleInstanceAutomationState::Error);
    const auto error = controller.errorString();
    QVERIFY(!error.isEmpty());
    const auto visibleText = [page](const QString &text) {
        for (const auto *label : page->findChildren<QLabel *>()) {
            if (label->isVisible() && label->text() == text)
                return true;
        }
        return false;
    };
    QTRY_VERIFY(visibleText(AutomationPage::tr("Error")));
    QTRY_VERIFY(visibleText(error));
    occupiedPort.close();
    clickOption(page, enabled);
    QTRY_COMPARE(coordinator.automationState().state,
                 SingleInstanceAutomationState::ServerDisabled);
    clickOption(page, enabled);
    QTRY_COMPARE(coordinator.automationState().state, SingleInstanceAutomationState::ServerReady);
    QTRY_VERIFY(visibleText(AutomationPage::tr("Server ready")));
    QVERIFY(!visibleText(error));
    QCOMPARE(qApp->property(EndpointProperty).toString(), configuredEndpoint());

    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy::NoProxy);
    const auto checkTrackAccess = [&](const bool allowed) {
        QNetworkRequest request{QUrl(configuredEndpoint())};
        request.setTransferTimeout(5000);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json, text/event-stream");
        request.setRawHeader("MCP-Protocol-Version", Mcp::ProtocolVersion);
        request.setRawHeader("Mcp-Method", Mcp::ToolsCallMethod);
        request.setRawHeader("Mcp-Name", "tracks.list");
        const auto message = Mcp::makeRequest(
            QString::fromLatin1(Mcp::ToolsCallMethod),
            {
                {QStringLiteral("name"),      QStringLiteral("tracks.list")                },
                {QStringLiteral("arguments"),
                 QJsonObject{{QStringLiteral("document_id"), before.documentId.toString()}}}
        },
            {}, 1);
        const QScopedPointer<QNetworkReply> reply(
            network.post(request, QJsonDocument(message).toJson(QJsonDocument::Compact)));
        QTRY_VERIFY(reply->isFinished());
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        const auto body = reply->readAll();
        const auto response = QJsonDocument::fromJson(body).object();
        QVERIFY2(response.contains(QStringLiteral("result")), body.constData());
        const auto result = response.value(QStringLiteral("result")).toObject();
        QCOMPARE(result.value(QStringLiteral("isError")).toBool(), !allowed);
        if (allowed) {
            QVERIFY2(result.value(QStringLiteral("structuredContent"))
                         .toObject()
                         .value(QStringLiteral("tracks"))
                         .isArray(),
                     body.constData());
        } else {
            QCOMPARE(result.value(QStringLiteral("structuredContent"))
                         .toObject()
                         .value(QStringLiteral("code"))
                         .toString(),
                     QStringLiteral("permission_denied"));
        }
    };
    checkTrackAccess(true);
    if (QTest::currentTestFailed())
        return;

    auto *level = page->findChild<ComboBox *>("automationControlLevel");
    QVERIFY(level);
    page->ensureWidgetVisible(level);
    selectComboIndex(level,
                     level->findData(static_cast<int>(AutomationOption::ControlLevel::Custom)));
    checkTrackAccess(false);
    if (QTest::currentTestFailed())
        return;
    clickOption(page, page->findChild<ToolButton *>("automationOpenToolsetButton"));
    clickOption(page, page->findChild<ToolButton *>("automationCustomToolGroupExpand_tracks"));
    clickOption(page, page->findChild<SwitchButton *>("automationCustomTool_tracks.list"));
    if (QTest::currentTestFailed())
        return;
    checkTrackAccess(true);
    if (QTest::currentTestFailed())
        return;
    clickOption(page, page->findChild<ToolButton *>("automationCloseToolsetButton"));

    QTcpServer nextPort;
    QVERIFY(nextPort.listen(QHostAddress::LocalHost, 0));
    const auto newPort = nextPort.serverPort();
    nextPort.close();
    clickOption(page, portEditor);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(portEditor->hasFocus());
    QApplication::clipboard()->setText(QString::number(newPort));
    QTest::keySequence(portEditor, QKeySequence::SelectAll);
    QTest::keySequence(portEditor, QKeySequence::Paste);
    QTest::keyClick(portEditor, Qt::Key_Return);
    QTRY_COMPARE(coordinator.automationState().state, SingleInstanceAutomationState::ServerReady);
    QTRY_COMPARE(qApp->property(EndpointProperty).toString(), configuredEndpoint());
    QCOMPARE(options->controlPort, newPort);
    checkTrackAccess(true);
    if (QTest::currentTestFailed())
        return;
    QTcpServer releasedPort;
    QVERIFY(releasedPort.listen(QHostAddress::LocalHost, port));

    clickOption(page, configCopy);
    if (QTest::currentTestFailed())
        return;
    QJsonParseError parseError;
    const auto config =
        QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8(), &parseError).object();
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(config.value(QStringLiteral("url")).toString(), configuredEndpoint());
    clickOption(page, endpointCopy);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(QApplication::clipboard()->text(), configuredEndpoint());
    clickOption(page, enabled);
    QTRY_COMPARE(coordinator.automationState().state,
                 SingleInstanceAutomationState::ServerDisabled);
    QVERIFY(qApp->property(EndpointProperty).toString().isEmpty());
    QTcpServer stoppedPort;
    QVERIFY(stoppedPort.listen(QHostAddress::LocalHost, newPort));

    Button *randomize = nullptr;
    for (auto *button : page->findChildren<Button *>()) {
        if (button->text() == AutomationPage::tr("Randomize"))
            randomize = button;
    }
    clickOption(page, randomize);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(options->controlPort != newPort);
    clickOption(page, endpointCopy);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(QApplication::clipboard()->text(), configuredEndpoint());
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
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
