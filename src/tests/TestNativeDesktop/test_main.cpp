#include "tst_native_desktop.h"
#include "Bootstrap/AppEnvironment.h"

#include <QtTest/QTest>

void NativeDesktopTests::initTestCase() {
    AppEnvironment::postInit(AppHostMode::Gui);
}

QTEST_MAIN(NativeDesktopTests)
