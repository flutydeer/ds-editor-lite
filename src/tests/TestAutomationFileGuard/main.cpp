#include "Automation/Public/AutomationFileGuard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    bool createFile(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("fixture") == 7;
    }

    bool hasError(const Automation::AutomationResult<Automation::AuthorizedPath> &result,
                  const Automation::AutomationErrorCode code) {
        return !result && result.getError().code == code;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    expect(temporary.isValid(), QStringLiteral("temporary root must be available"));
    if (!temporary.isValid())
        return 1;

    const auto readRoot = QDir(temporary.path()).filePath(QStringLiteral("read"));
    const auto writeRoot = QDir(temporary.path()).filePath(QStringLiteral("write"));
    const auto siblingRoot = QDir(temporary.path()).filePath(QStringLiteral("read-other"));
    expect(QDir().mkpath(readRoot) && QDir().mkpath(writeRoot) && QDir().mkpath(siblingRoot),
           QStringLiteral("fixture directories must be created"));

    const auto readable = QDir(readRoot).filePath(QStringLiteral("song.dspx"));
    const auto sibling = QDir(siblingRoot).filePath(QStringLiteral("outside.dspx"));
    const auto siblingNeighbor = QDir(siblingRoot).filePath(QStringLiteral("neighbor.dspx"));
    expect(createFile(readable) && createFile(sibling) && createFile(siblingNeighbor),
           QStringLiteral("fixture files must be created"));

    Automation::AutomationFileGuard guard;
    const auto configured = guard.setConfiguredRoots({readRoot}, {writeRoot});
    expect(configured.isPresent(), QStringLiteral("valid canonical roots must be accepted"));

    const auto allowedRead = guard.authorize(readable, Automation::FileAccessPurpose::Read);
    expect(allowedRead && QFileInfo(allowedRead.get().canonicalPath).canonicalFilePath() ==
                              QFileInfo(readable).canonicalFilePath(),
           QStringLiteral("existing file below a read root must be authorized canonically"));

    const auto missingRead = guard.authorize(
        QDir(readRoot).filePath(QStringLiteral("missing.dspx")),
        Automation::FileAccessPurpose::Read);
    expect(hasError(missingRead, Automation::AutomationErrorCode::FileNotFound),
           QStringLiteral("missing read target must remain file_not_found"));

    const auto siblingRead = guard.authorize(sibling, Automation::FileAccessPurpose::Read);
    expect(hasError(siblingRead, Automation::AutomationErrorCode::PermissionDenied),
           QStringLiteral("same-prefix sibling directory must not match a configured root"));

    const auto traversedRead = guard.authorize(
        QDir(readRoot).filePath(QStringLiteral("../read-other/outside.dspx")),
        Automation::FileAccessPurpose::Read);
    expect(hasError(traversedRead, Automation::AutomationErrorCode::PermissionDenied),
           QStringLiteral("parent traversal must be checked after canonicalization"));

    const auto output = QDir(writeRoot).filePath(QStringLiteral("nested/render.wav"));
    const auto allowedWrite = guard.authorize(output, Automation::FileAccessPurpose::Write);
    expect(allowedWrite && allowedWrite.get().canonicalPath.endsWith(
                               QStringLiteral("/write/nested/render.wav"), Qt::CaseInsensitive),
           QStringLiteral("nonexistent output must canonicalize from its nearest existing parent"));

    const auto deniedWrite = guard.authorize(
        QDir(readRoot).filePath(QStringLiteral("render.wav")),
        Automation::FileAccessPurpose::Write);
    expect(hasError(deniedWrite, Automation::AutomationErrorCode::PermissionDenied),
           QStringLiteral("read roots must not implicitly grant write access"));

    const auto grant = guard.addSessionGrant(sibling, Automation::FileAccessPurpose::Read);
    const auto grantedRead = guard.authorize(sibling, Automation::FileAccessPurpose::Read);
    const auto ungrantedNeighbor =
        guard.authorize(siblingNeighbor, Automation::FileAccessPurpose::Read);
    expect(grant && grantedRead &&
               hasError(ungrantedNeighbor, Automation::AutomationErrorCode::PermissionDenied),
           QStringLiteral("an exact session file grant must not widen into a directory grant"));

    const auto relative = guard.authorize(QStringLiteral("relative.dspx"),
                                          Automation::FileAccessPurpose::Read);
    expect(hasError(relative, Automation::AutomationErrorCode::InvalidArgument),
           QStringLiteral("relative paths must be rejected before policy matching"));

    const auto snapshot = guard.snapshot();
    expect(snapshot.readRoots.size() == 1 && snapshot.writeRoots.size() == 1 &&
               snapshot.sessionReadGrants.size() == 1 &&
               snapshot.sessionWriteGrants.isEmpty(),
           QStringLiteral("file access snapshot must report configured roots and session grants"));

    return failures == 0 ? 0 : 1;
}
