#include "Automation/OperationIds.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <type_traits>

static_assert(std::is_same_v<std::remove_cv_t<decltype(Automation::OperationIds::notes::insert)>,
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

    bool requireMatch(const SourceFile &file, const QRegularExpression &pattern,
                      const QString &rule) {
        if (pattern.match(file.contents).hasMatch())
            return true;
        QTextStream(stderr) << "FAILED: " << rule << " at " << file.relativePath << Qt::endl;
        return false;
    }

    bool requireMatchCount(const SourceFile &file, const QRegularExpression &pattern,
                           const qsizetype expectedCount, const QString &rule) {
        qsizetype count = 0;
        auto matches = pattern.globalMatch(file.contents);
        while (matches.hasNext()) {
            matches.next();
            ++count;
        }
        if (count == expectedCount)
            return true;
        QTextStream(stderr) << "FAILED: " << rule << " at " << file.relativePath << " (expected "
                            << expectedCount << ", found " << count << ')' << Qt::endl;
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
    const QRegularExpression versionedOperationSuffix(QStringLiteral(R"(\.v[0-9]+$)"));
    const QRegularExpression semanticNotePointer(
        QStringLiteral(R"(\b(?:onInsertNote|onSplitNote)\s*\([^;{}]*\bNote\s*\*)"));
    const QRegularExpression semanticNoteAllocation(QStringLiteral(R"(\bnew\s+Note\b)"));
    const QRegularExpression unconditionalNoteEditCommit(QStringLiteral(
        R"(\b(?:finishNoteEditSession|endActiveTransaction)\s*\(\s*EditSessionEndReason::Commit\s*\))"));
    const QRegularExpression directViewTrackColorWrite(QStringLiteral(
        R"(void\s+(?:TrackControlView|ChannelView)::setColorIndex\s*\([^)]*\)\s*\{[^{}]*\b(?:m_track|m_context)\s*->\s*setColorIndex\s*\()"));
    const QRegularExpression audioSourceChangedConnection(
        QStringLiteral(R"(&AudioClip::sourceChanged)"));
    const QRegularExpression audioSourceNotification(
        QStringLiteral(R"(\bnotifySourceChanged\s*\()"));
    const QRegularExpression audioLoadFailureRetryGate(
        QStringLiteral(R"(!m_unloadableAudioClips\.contains\s*\(\s*audioClip\s*\)\s*&&)"));
    const QRegularExpression insertedTrackAudioLifecycle(QStringLiteral(
        R"(if\s*\(\s*type\s*==\s*AppModel::Insert\s*\)\s*\{[\s\S]{0,800}connectClip\s*\(\s*audioClip\s*\)[\s\S]{0,300}startDecodingOrResolving\s*\(\s*audioClip\s*,\s*false\s*\))"));
    const QRegularExpression removedTrackAudioLifecycle(QStringLiteral(
        R"(else\s+if\s*\(\s*type\s*==\s*AppModel::Remove\s*\)\s*\{[\s\S]{0,800}disconnect\s*\(\s*static_cast<AudioClip\s*\*>\s*\(\s*clip\s*\)[\s\S]{0,300}terminateTasksByTrackId\s*\(\s*track->id\s*\(\s*\)\s*\))"));
    const QRegularExpression prematureAudioPathNormalization(QStringLiteral(
        R"(void\s+AudioDecodingController::startDecodingOrResolving\s*\([^)]*\)\s*\{[\s\S]{0,900}\bsetAudioClipPathStatus\s*\()"));
    const QRegularExpression resolutionProjectRootGate(
        QStringLiteral(R"(DocumentWorkflowPathUtils::projectPathsEqual\s*\()"));
    const QRegularExpression deferredAudioCompletionBeforeRemoval(QStringLiteral(
        R"(void\s+AudioDecodingController::handle(?:ResolveTask|CascadeResolveTask|Task)Finished\s*\([^)]*\)\s*\{[\s\S]{0,700}deferCompletionWhileDocumentBusy\s*\([\s\S]{0,700}taskManager->removeTask\s*\()"));
    const QRegularExpression deferredImportedAudioCompletionBeforeRemoval(QStringLiteral(
        R"(void\s+TrackController::handle(?:DecodeAudioTask|ComputeAudioHashTask)Finished\s*\([^)]*\)\s*\{[\s\S]{0,700}deferCompletionWhileDocumentBusy\s*\([\s\S]{0,700}taskManager->removeTask\s*\()"));
    const QRegularExpression decodeSuccessNormalizesStatus(QStringLiteral(
        R"(ProjectAutomationFacade::applyAudioDecodeCache[\s\S]{0,1800}setPathStatus\s*\(\s*AudioClip::PathStatus::Normal\s*\))"));
    const QRegularExpression resolvedAudioStatusBeforeSourceNotification(QStringLiteral(
        R"(ProjectAutomationFacade::applyResolvedAudioPath[\s\S]{0,2600}setPathStatus\s*\(\s*status\s*\)\s*;[\s\S]{0,120}setPath\s*\(\s*resolvedPath\s*\))"));
    const QRegularExpression audioSourceGeneration(QStringLiteral(R"(\bsourceGeneration\b)"));
    const QRegularExpression fillLyricLanguageCommit(QStringLiteral(
        R"(arg\.language\s*=\s*noteResult\.language\s*;[\s\S]{0,300}edit\.language\s*=\s*arg\.language\s*;)"));
    const QRegularExpression taggerDetailSavedBeforeReorder(QStringLiteral(
        R"(void\s+TaggerConfigTab::onOrderChanged\s*\(\s*\)\s*\{\s*saveCurrentDetail\s*\(\s*\)\s*;[\s\S]{0,160}m_listPanel->listWidget\s*\(\s*\))"));
    const QRegularExpression trackAutoPageTurnBinding(QStringLiteral(
        R"(&AppStatus::trackAutoPageTurnEnabledChanged[\s\S]{0,300}setAuto(?:PageTurn|TurnPage)\s*\(\s*enabled\s*\))"));
    const QRegularExpression pianoRollAutoPageTurnBinding(QStringLiteral(
        R"(&AppStatus::pianoRollAutoPageTurnEnabledChanged[\s\S]{0,300}setAuto(?:PageTurn|TurnPage)\s*\(\s*enabled\s*\))"));
    const QRegularExpression midiBatchUsesSharedPreparation(
        QStringLiteral(R"(MidiFilePreparer::prepare\s*\(\s*\{\s*path\s*\}\s*\))"));
    const QRegularExpression midiBatchForwardsPreparationFailure(
        QStringLiteral(R"(MidiFilePreparer::failureMessage\s*\(\s*item\s*\))"));

    for (const auto &id : Automation::OperationIds::all()) {
        if (!versionedOperationSuffix.match(id).hasMatch())
            continue;
        QTextStream(stderr) << "FAILED: In-process operation ID has a version suffix: " << id
                            << Qt::endl;
        ok = false;
    }

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
                QTextStream(stderr) << "FAILED: Product operation ID bypassed OperationIds at "
                                    << file.relativePath << ':' << line << Qt::endl;
                ok = false;
            }
        }

        ok &= rejectMatch(file, versionedAutomationContract,
                          QStringLiteral("Versioned in-process contract ID is not allowed"));

        ok &= rejectMatch(file, directViewTrackColorWrite,
                          QStringLiteral("Public view color setter bypassed TrackController"));

        if (file.relativePath == QStringLiteral("src/app/Controller/AudioDecodingController.cpp") ||
            file.relativePath == QStringLiteral("src/app/Modules/Audio/AudioContext.cpp")) {
            ok &= requireMatch(
                file, audioSourceChangedConnection,
                QStringLiteral("Audio source consumers stopped observing metadata-only relinks"));
        }

        if (file.relativePath == QStringLiteral("src/app/Modules/Audio/AudioContext.cpp")) {
            ok &= requireMatch(
                file, audioLoadFailureRetryGate,
                QStringLiteral("Failed audio source could be retried by unrelated properties"));
        }

        if (file.relativePath == QStringLiteral("src/app/Controller/AudioDecodingController.cpp")) {
            ok &= requireMatch(
                file, insertedTrackAudioLifecycle,
                QStringLiteral("Inserted track audio clips stopped entering the task lifecycle"));
            ok &= requireMatch(
                file, removedTrackAudioLifecycle,
                QStringLiteral("Removed track audio clips retained task or signal ownership"));
            ok &= rejectMatch(file, prematureAudioPathNormalization,
                              QStringLiteral("Audio path status normalized before decode success"));
            ok &= requireMatch(
                file, resolutionProjectRootGate,
                QStringLiteral("Audio resolution ignored a changed project directory"));
            ok &= requireMatchCount(
                file, deferredAudioCompletionBeforeRemoval, 3,
                QStringLiteral("Finished audio task bypassed document-busy deferral"));
        }

        if (file.relativePath == QStringLiteral("src/app/Controller/TrackController.cpp")) {
            ok &= requireMatchCount(
                file, deferredImportedAudioCompletionBeforeRemoval, 2,
                QStringLiteral("Finished imported-audio task bypassed document-busy deferral"));
        }

        if (file.relativePath == QStringLiteral("src/app/Automation/ProjectAutomationFacade.cpp")) {
            ok &= requireMatch(
                file, decodeSuccessNormalizesStatus,
                QStringLiteral("Successful audio decode stopped normalizing path status"));
            ok &= requireMatch(
                file, resolvedAudioStatusBeforeSourceNotification,
                QStringLiteral("Resolved audio status was published after source notification"));
        }

        if (file.relativePath == QStringLiteral("src/app/Automation/ProjectAutomationDtos.h") ||
            file.relativePath == QStringLiteral("src/libs/ProjectModel/AppModel/AudioClip.cpp")) {
            ok &= requireMatch(
                file, audioSourceGeneration,
                QStringLiteral("Audio async snapshots lost same-path source generation"));
        }

        if (file.relativePath ==
                QStringLiteral(
                    "src/app/Controller/Actions/AppModel/Clip/EditAudioClipPathAction.cpp") ||
            file.relativePath == QStringLiteral("src/app/Automation/ProjectAutomationFacade.cpp")) {
            ok &= requireMatch(
                file, audioSourceNotification,
                QStringLiteral("Same-path audio source update stopped notifying consumers"));
        }

        if (file.relativePath == QStringLiteral("src/app/Controller/ClipController.h")) {
            ok &= rejectMatch(
                file, semanticNotePointer,
                QStringLiteral("Piano-roll semantic mutation transferred raw Note ownership"));
        }

        if (file.relativePath == QStringLiteral("src/app/Controller/ClipController.cpp")) {
            ok &= requireMatch(
                file, fillLyricLanguageCommit,
                QStringLiteral("Fill Lyrics stopped committing the resolved note language"));
        }

        if (file.relativePath ==
            QStringLiteral("src/app/Modules/FillLyric/Widgets/TaggerConfigTab.cpp")) {
            ok &= requireMatch(
                file, taggerDetailSavedBeforeReorder,
                QStringLiteral("Tagger reorder discarded the active custom-rule editor state"));
        }

        if (file.relativePath ==
            QStringLiteral("src/app/UI/Views/TrackEditor/TrackEditorView.cpp")) {
            ok &= requireMatch(
                file, trackAutoPageTurnBinding,
                QStringLiteral("Track auto-page state stopped reaching the active editor backend"));
        }

        if (file.relativePath ==
            QStringLiteral("src/app/UI/Views/ClipEditor/PianoRoll/PianoRollView.cpp")) {
            ok &= requireMatch(
                file, pianoRollAutoPageTurnBinding,
                QStringLiteral(
                    "Piano-roll auto-page state stopped reaching the active editor backend"));
        }

        if (file.relativePath ==
            QStringLiteral("src/app/Modules/Import/DocumentImportController.cpp")) {
            ok &= requireMatch(
                file, midiBatchUsesSharedPreparation,
                QStringLiteral("MIDI batch import duplicated the shared preparation pipeline"));
            ok &= requireMatch(
                file, midiBatchForwardsPreparationFailure,
                QStringLiteral("MIDI batch preparation failure stopped reaching the summary"));
        }

        if (file.relativePath == QStringLiteral("src/app/UI/Views/ClipEditor/PianoRoll/"
                                                "PianoRollGraphicsViewHelper.cpp")) {
            ok &= rejectMatch(file, semanticNoteAllocation,
                              QStringLiteral("Piano-roll helper allocated a semantic Note"));
        }

        if (file.relativePath == QStringLiteral("src/app/UI/Views/ClipEditor/PianoRoll/"
                                                "PianoRollGraphicsViewHelper.cpp") ||
            file.relativePath == QStringLiteral("src/app/UI/Views/ClipEditor/PianoRoll/"
                                                "DrawNoteHandler.cpp") ||
            file.relativePath == QStringLiteral("src/app/UI/Views/ClipEditor/PianoRoll/"
                                                "PianoRollRhiWidget.cpp")) {
            ok &= rejectMatch(
                file, unconditionalNoteEditCommit,
                QStringLiteral("Note creation committed its edit session unconditionally"));
        }

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

        if (file.relativePath !=
            QStringLiteral("src/app/Automation/DocumentAutomationFacade.cpp")) {
            ok &= rejectMatch(file, generationReplacement,
                              QStringLiteral("Document generation bypassed Document Facade"));
            ok &= rejectMatch(file, modelReplacement,
                              QStringLiteral("Project replacement bypassed Document Facade"));
        }
    }

    return ok ? 0 : 1;
}
