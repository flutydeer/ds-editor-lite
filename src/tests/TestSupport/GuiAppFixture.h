#pragma once

#include "RuntimeResourcesFixture.h"

#include "AppContext.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/Tasking/TaskManager.h>
#include <TalcsDevice/AudioDevice.h>

#include <QApplication>
#include <QScreen>
#include <QTemporaryDir>
#include <QStringList>
#include <QtTest/QTest>

#include <memory>

namespace TestSupport {
    inline void placeWindowOnScreen(QWidget &window, const QSize &preferredSize) {
        // Keep native cursor input inside the desktop, including window decorations.
        const auto available = window.screen()->availableGeometry().adjusted(40, 40, -40, -40);
        window.resize(preferredSize.boundedTo(available.size()));
        window.move(available.topLeft());
    }
}

class GuiAppFixture final {
public:
    ~GuiAppFixture() {
        if (context) {
            // Queued task completions must reach their controllers before teardown.
            const auto drain = [] {
                QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
            };
            drain();
        }
        context.reset();
        restoreVariable("DSEL_TEST_DATA_ROOT", previousRoot);
        restoreVariable("DSEL_TEST_PLUGIN_ROOT", previousPlugins);
        QApplication::setQuitOnLastWindowClosed(previousQuitOnClose);
    }

    bool initialize(bool closeOutput = true, const QStringList &packageSearchPaths = {}) {
        if (!directory.isValid()) {
            error = QStringLiteral("Cannot create the application sandbox");
            return false;
        }
        qputenv("DSEL_TEST_DATA_ROOT", directory.path().toUtf8());
        if (!TestSupport::initializeApplicationResources()) {
            error = QStringLiteral("Application resources are unavailable");
            return false;
        }
        QApplication::setQuitOnLastWindowClosed(false);
        auto options = std::make_unique<AppOptions>();
        options->general()->packageSearchPaths = packageSearchPaths;
        options->general()->defaultSingingLanguage = QStringLiteral("eng");
        options->inference()->executionProvider = QStringLiteral("CPU");
        options->inference()->autoStartInfer = false;
        options->inference()->cacheDirectory = directory.filePath(QStringLiteral("cache"));
        options->appearance()->animationEnabled = false;
        context = std::make_unique<AppContext>(std::move(options), AppHostMode::Gui);
        if (closeOutput) {
            if (auto *device = AudioSystem::outputSystem()->context()->device()) {
                device->stop();
                device->close();
            }
        }
        if (!ThemeManager::instance()->initialize(ThemeIds::defaultThemeId())) {
            error = ThemeLoader::lastError();
            return false;
        }
        return context->initializeDefaultDocument(&error);
    }

    QTemporaryDir directory;
    QString error;
    std::unique_ptr<AppContext> context;

private:
    static void restoreVariable(const char *name, const QByteArray &value) {
        if (value.isNull())
            qunsetenv(name);
        else
            qputenv(name, value);
    }

    const QByteArray previousRoot = qgetenv("DSEL_TEST_DATA_ROOT");
    const QByteArray previousPlugins = qgetenv("DSEL_TEST_PLUGIN_ROOT");
    const bool previousQuitOnClose = QApplication::quitOnLastWindowClosed();
};

class GuiDocumentFixture final {
public:
    ~GuiDocumentFixture() {
        if (!context)
            return;
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        QVERIFY2(context->initializeDefaultDocument(&error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    }

    bool initialize(bool closeOutput = true) {
        context = AppContext::s_self;
        if (!context || !directory.isValid()) {
            error = QStringLiteral("The suite application or document sandbox is unavailable");
            return false;
        }
        if (auto *device = AudioSystem::outputSystem()->context()->device()) {
            if (closeOutput) {
                device->stop();
                device->close();
            } else if (!AudioSystem::outputSystem()->setDevice(device->name())) {
                error = QStringLiteral("Cannot reopen the application audio device");
                return false;
            }
        }
        return context->initializeDefaultDocument(&error);
    }

    QTemporaryDir directory;
    QString error;
    AppContext *context = nullptr;
};
