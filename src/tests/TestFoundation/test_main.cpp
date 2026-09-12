#include "tst_foundation.h"

#include <QtTest/QTest>
#include <QCoreApplication>

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--log-fixture"))
        return runLogFixture(arguments.at(2));
    FoundationTests tests;
    return QTest::qExec(&tests, argc, argv);
}
