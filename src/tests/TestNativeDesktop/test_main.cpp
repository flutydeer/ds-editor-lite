#include "tst_native_desktop.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppStatus/AppStatus.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#include <QtTest/QTest>

NativeDesktopTests::NativeDesktopTests() = default;
NativeDesktopTests::~NativeDesktopTests() = default;

void NativeDesktopTests::runIsolatedDesktopCase() {
    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("DSEL_TEST_GUI_LIFECYCLE"), QStringLiteral("1"));
    child.setProcessEnvironment(environment);
    child.setProcessChannelMode(QProcess::MergedChannels);
    auto testCase = QString::fromLatin1(QTest::currentTestFunction());
    if (const auto *tag = QTest::currentDataTag(); tag && *tag)
        testCase += ':' + QString::fromLatin1(tag);
    child.start(QCoreApplication::applicationFilePath(), {testCase, QStringLiteral("-v1")});
    QVERIFY2(child.waitForStarted(), qPrintable(child.errorString()));
    const auto completed = child.waitForFinished(20000);
    const auto output = child.readAll();
    QVERIFY2(completed, output.constData());
    QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
             qPrintable(QStringLiteral("Child exit code %1:\n%2")
                            .arg(child.exitCode())
                            .arg(QString::fromUtf8(output))));
}

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
