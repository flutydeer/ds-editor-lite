#include "tst_application_gui.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <TalcsDevice/AudioDevice.h>

#include <QtTest/QTest>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDialog>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTimer>

ApplicationGuiTests::ApplicationGuiTests() = default;
ApplicationGuiTests::~ApplicationGuiTests() = default;

void ApplicationGuiTests::initTestCase() {
    QVERIFY(dataRoot.isValid());
    previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
    qputenv("DSEL_TEST_DATA_ROOT", dataRoot.path().toUtf8());
    dataRootInstalled = true;
    QCOMPARE(AppDataPaths::testRoot(), QDir::cleanPath(dataRoot.path()));
    QApplication::setQuitOnLastWindowClosed(false);
    AppEnvironment::postInit(AppHostMode::Gui);

    auto options = std::make_unique<AppOptions>();
    QVERIFY(QDir::cleanPath(options->configPath()).startsWith(dataRoot.path() + '/'));
    options->general()->packageSearchPaths.clear();
    options->general()->defaultSingingLanguage = QStringLiteral("eng");
    options->inference()->autoStartInfer = false;
    options->inference()->executionProvider = QStringLiteral("CPU");
    options->inference()->cacheDirectory = dataRoot.filePath(QStringLiteral("cache"));
    options->appearance()->animationEnabled = false;
    context = std::make_unique<AppContext>(std::move(options), AppHostMode::Gui);
    // Close the fixture-owned stream so playback failure is independent of host devices.
    if (auto *device = AudioSystem::outputSystem()->context()->device()) {
        device->stop();
        device->close();
        QVERIFY(!device->isOpen());
        QVERIFY(!device->isStarted());
    }
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY2(ThemeManager::instance()->initialize(ThemeIds::defaultThemeId()),
             qPrintable(ThemeLoader::lastError()));
    savedClipboard = std::make_unique<QMimeData>();
    if (const auto *mime = QApplication::clipboard()->mimeData()) {
        for (const auto &format : mime->formats())
            savedClipboard->setData(format, mime->data(format));
    }
}

void ApplicationGuiTests::init() {
    QString error;
    QVERIFY2(context->initializeDefaultDocument(&error), qPrintable(error));
    historyManager->reset();
}

void ApplicationGuiTests::publicPlaybackDeviceFailureDoesNotOpenAModalDialog() {
    auto &runtime = *context->m_coreRuntime;
    const auto *device = AudioSystem::outputSystem()->context()->device();
    QVERIFY(!device || !device->isOpen());
    const auto before = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    QVERIFY(before);
    QVERIFY(before.get().playable);
    QCOMPARE(before.get().state, Automation::PlaybackState::Stopped);
    bool dialogShown = false;
    QTimer dismissUnexpectedDialog;
    connect(&dismissUnexpectedDialog, &QTimer::timeout, this, [&] {
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            dialogShown = true;
            dialog->reject();
        }
    });
    dismissUnexpectedDialog.start(10);
    auto command = commandContext();
    command.source = Automation::InvocationSource::PublicMcp;
    const auto result = runtime.playback().play(command);
    dismissUnexpectedDialog.stop();
    QVERIFY(!dialogShown);
    QVERIFY(!result);
    QCOMPARE(result.getError().code, Automation::AutomationErrorCode::HostCapabilityUnavailable);
    const auto after = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    QVERIFY(after);
    QCOMPARE(after.get().state, before.get().state);
    QCOMPARE(after.get().position, before.get().position);
    QCOMPARE(after.get().lastPosition, before.get().lastPosition);
    QCOMPARE(after.get().document, before.get().document);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::cleanup() {
    if (view) {
        view->setDataContext(nullptr);
        view.reset();
    }
    scene.reset();
    clipController->setClip(nullptr);
    singingClip = nullptr;
}

void ApplicationGuiTests::cleanupTestCase() {
    if (savedClipboard)
        QApplication::clipboard()->setMimeData(savedClipboard.release());
    if (context) {
        context.reset();
    }
    if (dataRootInstalled) {
        if (previousDataRoot.isEmpty())
            qunsetenv("DSEL_TEST_DATA_ROOT");
        else
            qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
    }
}

Automation::CommandContext ApplicationGuiTests::commandContext() const {
    return {.expected = context->m_coreRuntime->documentVersion(),
            .source = Automation::InvocationSource::Test};
}

QTEST_MAIN(ApplicationGuiTests)
