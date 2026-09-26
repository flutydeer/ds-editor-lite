#include "tst_native_desktop.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppStatus/AppStatus.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMouseEvent>
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
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::Shortcut) {
        auto detail =
            QStringLiteral("%1/%2 [%3] event=%4 spontaneous=%5")
                .arg(object->parent()
                         ? QString::fromLatin1(object->parent()->metaObject()->className())
                         : QString{},
                     QString::fromLatin1(object->metaObject()->className()), object->objectName())
                .arg(event->type())
                .arg(event->spontaneous());
        if (auto *key = dynamic_cast<QKeyEvent *>(event))
            detail += QStringLiteral(" key=%1 modifiers=%2")
                          .arg(key->key())
                          .arg(key->modifiers().toInt());
        if (auto *mouse = dynamic_cast<QMouseEvent *>(event))
            detail += QStringLiteral(" button=%1 local=%2,%3 global=%4,%5")
                          .arg(mouse->button())
                          .arg(mouse->position().x())
                          .arg(mouse->position().y())
                          .arg(mouse->globalPosition().x())
                          .arg(mouse->globalPosition().y());
        recentInput.append(detail);
        if (recentInput.size() > 12)
            recentInput.removeFirst();
    }
    if (event->type() == QEvent::Show) {
        if (auto *message = qobject_cast<QMessageBox *>(object)) {
            const auto diagnostic = QStringLiteral("Unexpected message box: %1: %2")
                                        .arg(message->windowTitle(), message->text()) +
                                    '\n' + recentInput.join('\n');
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
