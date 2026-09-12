#include "tst_project_editing.h"

#include <QtTest>

int runProjectQuantizeProbe(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--quantize-probe"))
        return runProjectQuantizeProbe(argc, argv);
    QCoreApplication application(argc, argv);
    ProjectEditingTests tests;
    return QTest::qExec(&tests, argc, argv);
}
