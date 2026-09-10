#pragma once

#include "../TestSupport/RuntimeResourcesFixture.h"

#include "AppContext.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <TalcsDevice/AudioDevice.h>

#include <QApplication>
#include <QTemporaryDir>

#include <memory>

class NativeAppFixture final {
public:
    ~NativeAppFixture() {
        context.reset();
        restoreVariable("DSEL_TEST_DATA_ROOT", previousRoot);
        restoreVariable("DSEL_TEST_PLUGIN_ROOT", previousPlugins);
        QApplication::setQuitOnLastWindowClosed(previousQuitOnClose);
    }

    bool initialize(bool closeOutput = true) {
        if (!directory.isValid()) {
            error = QStringLiteral("Cannot create the application sandbox");
            return false;
        }
        qputenv("DSEL_TEST_DATA_ROOT", directory.path().toUtf8());
        if (!TestSupport::initializeApplicationResources()) {
            error = QStringLiteral("Application resources are unavailable");
            return false;
        }
        AppEnvironment::postInit(AppHostMode::Gui);
        QApplication::setQuitOnLastWindowClosed(false);
        auto options = std::make_unique<AppOptions>();
        options->general()->packageSearchPaths.clear();
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
