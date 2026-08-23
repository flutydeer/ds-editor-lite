#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    class Suite final {
    public:
        template <typename Function>
        void run(const QString &name, Function function) {
            m_current = name;
            ++m_scenarios;
            const auto failuresBefore = m_failures;
            function();
            if (m_failures == failuresBefore)
                ++m_passed;
        }

        void expect(const bool condition, const QString &message) {
            ++m_assertions;
            if (condition)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_current << "]: " << message << Qt::endl;
        }

        [[nodiscard]] int finish() const {
            QTextStream(stdout) << "MIDI import automation: " << m_scenarios << " scenarios, "
                                << m_passed << " passed, " << m_assertions << " assertions, "
                                << m_failures << " failures" << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_current;
        int m_scenarios = 0;
        int m_passed = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    [[nodiscard]] std::string utf8(const QString &text) {
        return text.toUtf8().toStdString();
    }

    [[nodiscard]] MidiParseData makeParsedMidi() {
        using Intermediate = opendspx::MidiIntermediateData;

        MidiParseData result;
        result.path = QStringLiteral("MIDI_ONE");
        result.valid = true;
        result.mediate = Intermediate(480,
                                      {
                                          {0, 87.0}
        },
                                      {{0, 6, 4}}, {},
                                      {
                                          {utf8(QStringLiteral("英语")),
                                           {
                                               {0, 480, 60, utf8(QStringLiteral("你好"))},
                                               {960, 240, 64, "world"},
                                           },
                                           0,
                                           1440},
                                          {utf8(QStringLiteral("日语")),
                                           {
                                               {240, 360, 58, utf8(QStringLiteral("世界"))},
                                           },
                                           1,
                                           960},
                                      });
        result.trackInfos = buildMidiTrackInfoList(result.mediate.tracks());
        return result;
    }

    [[nodiscard]] MidiGenerationResult generateSelectedTrack(MidiParseData &data) {
        MidiImportOptions options;
        options.codec = QByteArrayLiteral("UTF-8");
        options.selectedTrackIndices = {0};
        options.importTempo = true;
        options.importTimeSignature = true;
        return MidiTrackGenerator::generateTracks(data, options, QStringLiteral("mandarin"),
                                                  QStringLiteral("啦"), Timeline());
    }

    [[nodiscard]] Automation::DocumentDraftDto makeImportDraft(MidiGenerationResult &generated) {
        AppModel source;
        auto timeline = source.timeline();
        timeline.setTempos(generated.tempos);
        timeline.setTimeSignatures(generated.timeSignatures);
        source.setTimeline(std::move(timeline));
        for (auto *track : generated.tracks)
            source.insertTrack(track, source.tracks().size());
        generated.tracks.clear();
        return Automation::documentDraftDto(source.takeProjectData());
    }

    [[nodiscard]] Automation::CommandContext context(const Automation::CoreRuntime &runtime,
                                                     const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    void testSelectionAndGeometry(Suite &suite) {
        suite.run(QStringLiteral("selected-track-generates-valid-automation-draft"), [&] {
            auto parsed = makeParsedMidi();
            auto generated = generateSelectedTrack(parsed);
            suite.expect(generated.errorMessage.isEmpty() && generated.tracks.size() == 1,
                         QStringLiteral("only the selected MIDI track must be generated"));
            suite.expect(generated.hasTimeline && generated.tempos.size() == 1 &&
                             generated.timeSignatures.size() == 1,
                         QStringLiteral("selected import options must preserve the MIDI timeline"));

            const auto draft = makeImportDraft(generated);
            suite.expect(
                draft.tracks.size() == 1 && draft.tracks.first().name == QStringLiteral("英语") &&
                    draft.tracks.first().clips.size() == 1,
                QStringLiteral("track selection and Unicode metadata must survive generation"));
            const auto &clip = draft.tracks.first().clips.first();
            suite.expect(clip.notes.size() == 2 &&
                             clip.notes.first().lyric == QStringLiteral("你好"),
                         QStringLiteral("Unicode lyrics must survive UTF-8 conversion"));
            suite.expect(clip.properties.clipStart + clip.properties.clipLen <=
                             clip.properties.length,
                         QStringLiteral("visible MIDI clip geometry must fit its material length"));
            const auto validation = Automation::validate(draft);
            suite.expect(static_cast<bool>(validation),
                         QStringLiteral("generated MIDI document must satisfy Facade validation"));
        });

        suite.run(QStringLiteral("invalid-track-selection-is-rejected"), [&] {
            auto parsed = makeParsedMidi();
            MidiImportOptions options;
            options.codec = QByteArrayLiteral("UTF-8");
            options.selectedTrackIndices = {2};
            const auto generated = MidiTrackGenerator::generateTracks(
                parsed, options, QStringLiteral("mandarin"), QStringLiteral("啦"), Timeline());
            suite.expect(
                !generated.errorMessage.isEmpty() && generated.tracks.isEmpty(),
                QStringLiteral("out-of-range selection must fail without generated objects"));
        });
    }

    void testFacadeCommitUndoRedo(Suite &suite) {
        AppModel destination;
        destination.newProject();
        auto *history = HistoryManager::instance();
        history->reset(HistoryManager::ResetState::Saved);
        Automation::CoreRuntime runtime(&destination, history);

        auto parsed = makeParsedMidi();
        auto generated = generateSelectedTrack(parsed);
        const auto draft = makeImportDraft(generated);
        const auto initialTrackCount = destination.tracks().size();
        const auto initialTimeline = destination.timeline();

        suite.run(QStringLiteral("validate-only-predicts-without-side-effects"), [&] {
            const auto base = runtime.documentVersion();
            const auto result = runtime.documents().commitImportedDocument(context(runtime, true),
                                                                           draft, true, true);
            suite.expect(result && result.get().validatedOnly && result.get().changed &&
                             result.get().previous == base &&
                             result.get().current.revision == base.revision + 1,
                         QStringLiteral("validate-only must predict one atomic revision"));
            suite.expect(runtime.documentVersion() == base &&
                             destination.tracks().size() == initialTrackCount &&
                             !history->canUndo(),
                         QStringLiteral("validate-only must not mutate model or History"));
        });

        suite.run(QStringLiteral("commit-is-one-history-and-one-revision"), [&] {
            const auto base = runtime.documentVersion();
            const auto result =
                runtime.documents().commitImportedDocument(context(runtime), draft, true, true);
            suite.expect(result && result.get().changed &&
                             result.get().current.revision == base.revision + 1 &&
                             runtime.documentVersion() == result.get().current,
                         QStringLiteral("MIDI import must commit in one revision"));
            suite.expect(
                destination.tracks().size() == initialTrackCount + 1 && history->canUndo(),
                QStringLiteral("MIDI import must append one selected track and one History item"));
            suite.expect(!history->isOnSavePoint(),
                         QStringLiteral("committed MIDI import must leave the savepoint"));
            suite.expect(destination.timeline().tempos() == draft.timeline.tempos() &&
                             destination.timeline().timeSignatures() ==
                                 draft.timeline.timeSignatures(),
                         QStringLiteral("confirmed timeline options must be committed atomically"));
            const auto *track = destination.tracks().last();
            const auto *clip = track && track->clips().count() > 0
                                   ? dynamic_cast<SingingClip *>(*track->clips().begin())
                                   : nullptr;
            suite.expect(
                clip && clip->clipStart() + clip->clipLen() <= clip->length() &&
                    clip->notes().count() == 2,
                QStringLiteral("committed singing clip must retain valid geometry and notes"));
        });

        suite.run(QStringLiteral("single-undo-redo-restores-complete-import"), [&] {
            const auto undo = runtime.history().undo(context(runtime));
            suite.expect(undo && undo.get().changed &&
                             destination.tracks().size() == initialTrackCount &&
                             destination.timeline() == initialTimeline && history->canRedo() &&
                             history->isOnSavePoint(),
                         QStringLiteral("one undo must remove the track and restore the timeline"));

            const auto redo = runtime.history().redo(context(runtime));
            suite.expect(redo && redo.get().changed &&
                             destination.tracks().size() == initialTrackCount + 1 &&
                             destination.timeline().tempos() == draft.timeline.tempos() &&
                             !history->canRedo() && !history->isOnSavePoint(),
                         QStringLiteral("one redo must restore the complete import"));
        });

        history->reset();
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Suite suite;
    testSelectionAndGeometry(suite);
    testFacadeCommitUndoRedo(suite);
    return suite.finish();
}
