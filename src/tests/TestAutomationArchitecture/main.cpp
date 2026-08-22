#include "Automation/OperationIds.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <type_traits>

static_assert(std::is_same_v<
              std::remove_cv_t<decltype(Automation::OperationIds::notes::insert)>,
              QLatin1StringView>);

namespace {
    struct SourceFile {
        QString relativePath;
        QString contents;
    };

    QList<SourceFile> readSources(const QString &sourceRoot, bool &ok) {
        QList<SourceFile> result;
        const QDir root(sourceRoot);
        QDirIterator it(root.filePath(QStringLiteral("src/app")),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.hpp")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile file(it.next());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream(stderr) << "FAILED: cannot read " << file.fileName() << Qt::endl;
                ok = false;
                continue;
            }
            result.append({
                .relativePath = root.relativeFilePath(file.fileName()).replace(u'\\', u'/'),
                .contents = QString::fromUtf8(file.readAll()),
            });
        }
        return result;
    }

    bool rejectMatch(const SourceFile &file, const QRegularExpression &pattern,
                     const QString &rule) {
        const auto match = pattern.match(file.contents);
        if (!match.hasMatch())
            return true;
        const auto line = file.contents.first(match.capturedStart()).count(u'\n') + 1;
        QTextStream(stderr) << "FAILED: " << rule << " at " << file.relativePath << ':' << line
                            << Qt::endl;
        return false;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    const auto files = readSources(QString::fromUtf8(LITE_SOURCE_ROOT), ok);

    const QRegularExpression globalRuntimeAccess(QStringLiteral(
        R"(\b(?:appModel|historyManager|appStatus|appOptions|taskManager)\s*->|AppContext::instance\s*<|TaskManager::instance\s*\()"));
    const QRegularExpression historyRecord(QStringLiteral(
        R"(\b(?:history|historyManager|m_history|m_historyManager)\s*->\s*record\s*\()"));
    const QRegularExpression historyMutation(QStringLiteral(
        R"(\b(?:history|historyManager|m_history|m_historyManager)\s*->\s*(?:undo|redo|reset)\s*\()"));
    const QRegularExpression actionInclude(
        QStringLiteral(R"(#\s*include\s*[<"][^">\r\n]*Controller/Actions/AppModel/)"));
    const QRegularExpression stableStatusWrite(QStringLiteral(
        R"(\b(?:appStatus|m_appStatus|status)\s*->\s*(?:selectedTrackIndex|activeClipId|selectedNotes|selectedClips|pianoRollQuantize(?:Enabled)?|trackAutoPageTurnEnabled|pianoRollAutoPageTurnEnabled|loopSettings)\s*(?:\.set\s*\(|=(?!=)))"));
    const QRegularExpression revisionAdvance(
        QStringLiteral(R"(\bsession\s*\.\s*advanceRevision\s*\()"));
    const QRegularExpression generationReplacement(
        QStringLiteral(R"(\bsession\s*\.\s*replaceGeneration\s*\()"));
    const QRegularExpression modelReplacement(
        QStringLiteral(R"(\bmodel\s*->\s*replaceProject\s*\()"));
    const QRegularExpression versionedAutomationContract(
        QStringLiteral(R"("automation\.[A-Za-z0-9_]+\.v[0-9]+")"));

    for (const auto &file : files) {
        if (file.relativePath.startsWith(QStringLiteral("src/app/Automation/"))) {
            ok &= rejectMatch(file, globalRuntimeAccess,
                              QStringLiteral("Automation code accessed a global current runtime"));
        }

        if (file.relativePath != QStringLiteral("src/app/Automation/OperationIds.h")) {
            for (const auto &id : Automation::OperationIds::all()) {
                const auto literal = QStringLiteral("\"") + id + QStringLiteral("\"");
                const auto offset = file.contents.indexOf(literal);
                if (offset < 0)
                    continue;
                const auto line = file.contents.first(offset).count(u'\n') + 1;
                QTextStream(stderr)
                    << "FAILED: Product operation ID bypassed OperationIds at "
                    << file.relativePath << ':' << line << Qt::endl;
                ok = false;
            }
        }

        ok &= rejectMatch(file, versionedAutomationContract,
                          QStringLiteral("Versioned in-process contract ID is not allowed"));

        if (file.relativePath != QStringLiteral("src/app/Automation/CommandCommitter.cpp")) {
            ok &= rejectMatch(file, historyRecord,
                              QStringLiteral("History record bypassed CommandCommitter"));
        }

        if (file.relativePath != QStringLiteral("src/app/Automation/CommandCommitter.cpp") &&
            file.relativePath !=
                QStringLiteral("src/app/Automation/DocumentAutomationFacade.cpp")) {
            ok &= rejectMatch(file, historyMutation,
                              QStringLiteral("History mutation bypassed Automation runtime"));
        }

        if (!file.relativePath.startsWith(QStringLiteral("src/app/Automation/")) &&
            !file.relativePath.startsWith(QStringLiteral("src/app/Controller/Actions/"))) {
            ok &= rejectMatch(file, actionInclude,
                              QStringLiteral("Business action leaked outside Facade/Actions"));
        }

        if (file.relativePath != QStringLiteral("src/app/AppContext.cpp")) {
            ok &= rejectMatch(file, stableStatusWrite,
                              QStringLiteral("Stable GUI state bypassed its Facade host adapter"));
        }

        if (file.relativePath != QStringLiteral("src/app/Automation/CommandCommitter.cpp")) {
            ok &= rejectMatch(file, revisionAdvance,
                              QStringLiteral("Document revision bypassed CommandCommitter"));
        }

        if (file.relativePath != QStringLiteral("src/app/Automation/DocumentAutomationFacade.cpp")) {
            ok &= rejectMatch(file, generationReplacement,
                              QStringLiteral("Document generation bypassed Document Facade"));
            ok &= rejectMatch(file, modelReplacement,
                              QStringLiteral("Project replacement bypassed Document Facade"));
        }
    }

    return ok ? 0 : 1;
}
