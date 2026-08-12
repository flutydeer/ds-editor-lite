#include "Bootstrap/OpenDocumentRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    OpenDocumentRegistry registry;
    const int firstOwner = 1;
    const int secondOwner = 2;
    const auto firstPath = QDir::current().absoluteFilePath(QStringLiteral("projects/first.dspx"));
    const auto equivalentFirstPath =
        QDir::cleanPath(QDir::current().absoluteFilePath(QStringLiteral("./projects/first.dspx")));
    const auto secondPath =
        QDir::current().absoluteFilePath(QStringLiteral("projects/second.dspx"));

    bool ok = true;
    ok &= expect(registry.reserve(&firstOwner, firstPath), "first path reservation must succeed");
    ok &= expect(registry.ownerForPath(equivalentFirstPath) == &firstOwner,
                 "equivalent normalized paths must resolve to the same owner");
    ok &= expect(!registry.reserve(&secondOwner, equivalentFirstPath),
                 "a path cannot be reserved by two documents");
    ok &= expect(registry.reserve(&secondOwner, secondPath),
                 "an independent path reservation must succeed");
    ok &= expect(!registry.update(&secondOwner, firstPath),
                 "save-as cannot claim another open document's path");
    ok &= expect(registry.update(&firstOwner, secondPath) == false,
                 "updating to an occupied path must fail");

    registry.release(&secondOwner);
    ok &= expect(registry.update(&firstOwner, secondPath), "released paths must become available");
    ok &= expect(registry.ownerForPath(firstPath) == nullptr,
                 "updating a document path must release its previous path");
    ok &= expect(registry.pathForOwner(&firstOwner) == QDir::cleanPath(secondPath),
                 "the registry must retain the normalized current path");

    registry.release(&firstOwner);
    ok &= expect(registry.ownerForPath(secondPath) == nullptr,
                 "closing a document must release its path");
    return ok ? 0 : 1;
}
