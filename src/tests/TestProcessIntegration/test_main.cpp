#include "tst_process_integration.h"

#include <QCoreApplication>
#include <QtTest>

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    ProcessIntegrationTests tests;
    auto arguments = application.arguments();
    for (qsizetype index = 1; index < arguments.size();) {
        const auto option = arguments.at(index);
        if (option == QStringLiteral("--editor") || option == QStringLiteral("--connector") ||
            option == QStringLiteral("--platform-plugins")) {
            if (index + 1 >= arguments.size())
                return 2;
            arguments.removeAt(index);
            const auto value = arguments.takeAt(index);
            if (option == QStringLiteral("--editor"))
                tests.editorPath = value;
            else if (option == QStringLiteral("--connector"))
                tests.connectorPath = value;
            else
                tests.platformPluginDirectory = value;
        } else {
            ++index;
        }
    }
    if (tests.editorPath.isEmpty() || tests.connectorPath.isEmpty() ||
        tests.platformPluginDirectory.isEmpty()) {
        qCritical("Use --editor, --connector, and --platform-plugins to locate the built products");
        return 2;
    }
    return QTest::qExec(&tests, arguments);
}
