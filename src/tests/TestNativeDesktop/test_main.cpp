#include "tst_native_desktop.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppStatus/AppStatus.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>

#include <QtTest/QTest>

NativeDesktopTests::NativeDesktopTests() = default;
NativeDesktopTests::~NativeDesktopTests() = default;

bool NativeDesktopTests::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::Show) {
        if (auto *message = qobject_cast<QMessageBox *>(object)) {
            const auto diagnostic = QStringLiteral("Unexpected message box: %1: %2")
                                        .arg(message->windowTitle(), message->text());
            QTest::qFail(qPrintable(diagnostic), __FILE__, __LINE__);
            // Close after exec() enters its event loop so unattended runs can finish.
            QTimer::singleShot(0, message, [message] { message->reject(); });
        }
    }
    return QObject::eventFilter(object, event);
}

void NativeDesktopTests::initTestCase() {
    qApp->installEventFilter(this);
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
    qApp->removeEventFilter(this);
}

QTEST_MAIN(NativeDesktopTests)
