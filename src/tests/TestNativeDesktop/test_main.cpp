#include "tst_native_desktop.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppStatus/AppStatus.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <QFileInfo>

#include <QtTest/QTest>

NativeDesktopTests::NativeDesktopTests() = default;
NativeDesktopTests::~NativeDesktopTests() = default;

void NativeDesktopTests::initTestCase() {
    AppEnvironment::postInit(AppHostMode::Gui);
    if (qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE"))
        return;
    const auto root = TestSupport::voicebankRoot();
    QVERIFY2(QFileInfo(root).isAbsolute() && QFileInfo(root).isDir(), qPrintable(root));
    application = std::make_unique<GuiAppFixture>();
    QVERIFY2(application->initialize(true, {root}), qPrintable(application->error));
    packageManager->initialize({root});
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
}

void NativeDesktopTests::cleanupTestCase() {
    application.reset();
}

QTEST_MAIN(NativeDesktopTests)
