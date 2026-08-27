#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/AutomationWire/PublicConstants.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>

#include <QCoreApplication>
#include <QHash>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>
#include <QVersionNumber>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

namespace {
    using Automation::AutomationErrorCode;
    using Automation::AutomationResult;
    using Automation::ClipId;
    using Automation::CommandContext;
    using Automation::CoreRuntime;
    using Automation::MutationResult;
    using Automation::NoteId;
    using Automation::OperationId;
    using Automation::TrackId;

    class Suite final {
    public:
        template <typename Function>
        void run(const OperationId &operationId, const QString &dimension, Function function) {
            const auto id = scenarioId(operationId, dimension);
            if (m_ids.contains(id)) {
                fail(QStringLiteral("duplicate stable scenario ID: %1").arg(id));
                return;
            }
            m_ids.insert(id);
            m_current = id;
            ++m_scenarios;
            ++m_operationScenarios[operationId];
            const auto failuresBefore = m_failures;
            function();
            const bool passed = failuresBefore == m_failures;
            if (passed)
                ++m_passed;
            QTextStream(stdout) << "SCENARIO " << id << ' ' << (passed ? "PASS" : "FAIL")
                                << Qt::endl;
        }

        void expect(const bool condition, const QString &message) {
            ++m_assertions;
            if (!condition)
                fail(message);
        }

        void requireOperations(const QList<OperationId> &operations) {
            m_current = QStringLiteral("AFC-EDITDIM-OPERATION-MANIFEST");
            const auto failuresBefore = m_failures;
            QSet<OperationId> expected;
            for (const auto &operation : operations)
                expected.insert(operation);
            QSet<OperationId> actual;
            const auto countedOperations = m_operationScenarios.keys();
            for (const auto &operation : countedOperations)
                actual.insert(operation);

            expect(operations.size() == 40 && expected.size() == 40,
                   QStringLiteral("editing operation manifest must contain 40 unique handlers"));
            expect(actual == expected,
                   QStringLiteral("counted operation set must exactly match the editing manifest"));
            for (const auto &operation : operations) {
                const auto count = m_operationScenarios.value(operation);
                expect(count >= 6, QStringLiteral("%1 has only %2 applicable behavior scenarios")
                                       .arg(operation)
                                       .arg(count));
                expect(count <= 7, QStringLiteral("%1 unexpectedly counted %2 behavior scenarios")
                                       .arg(operation)
                                       .arg(count));
            }
            QTextStream(stdout) << "MANIFEST AFC-EDITDIM-OPERATION-MANIFEST "
                                << (failuresBefore == m_failures ? "PASS" : "FAIL") << Qt::endl;
        }

        int result() const {
            QTextStream output(stdout);
            output << "Automation editing dimensions: " << m_scenarios << " scenarios, " << m_passed
                   << " passed, " << m_assertions << " assertions, " << m_failures << " failures"
                   << Qt::endl;
            QList<OperationId> operations = m_operationScenarios.keys();
            std::sort(operations.begin(), operations.end());
            for (const auto &operation : operations)
                output << "OPERATION " << operation << ' ' << m_operationScenarios.value(operation)
                       << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        static QString scenarioId(const OperationId &operationId, const QString &dimension) {
            auto operation = operationId.toUpper();
            operation.replace('.', '-');
            operation.replace('_', '-');
            return QStringLiteral("AFC-EDITDIM-%1-%2").arg(operation, dimension);
        }

        void fail(const QString &message) {
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_current << "]: " << message << Qt::endl;
        }

        QString m_current;
        QSet<QString> m_ids;
        QHash<OperationId, int> m_operationScenarios;
        int m_scenarios = 0;
        int m_passed = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    CommandContext commandContext(const CoreRuntime &runtime, const bool validateOnly = false,
                                  const QString &idempotencyKey = {}) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .idempotencyKey = idempotencyKey,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::TrackDraftDto trackDraft(const QString &name, const QString &clientRef = {}) {
        return {
            .clientRef = clientRef,
            .name = name,
            .colorIndex = 2,
            .gain = 0.25,
            .pan = -0.125,
            .defaultLanguage = QStringLiteral("en"),
        };
    }

    Automation::ClipDraftDto singingClipDraft(const QString &name, const QString &clientRef = {}) {
        Automation::ClipDraftDto draft;
        draft.clientRef = clientRef;
        draft.type = Automation::ClipDraftDto::Type::Singing;
        draft.properties.name = name;
        draft.properties.length = 3840;
        draft.properties.clipLen = 3840;
        draft.properties.gain = 1.0;
        draft.defaultLanguage = QStringLiteral("en");
        return draft;
    }

    Automation::NoteDraftDto noteDraft(const int start, const int length, const int key,
                                       const QString &lyric, const QString &clientRef = {}) {
        Automation::NoteDraftDto draft;
        draft.clientRef = clientRef;
        draft.localStart = start;
        draft.length = length;
        draft.keyIndex = key;
        draft.lyric = lyric;
        draft.language = QStringLiteral("en");
        return draft;
    }

    SpeakerInfo speaker(const QString &id) {
        return SpeakerInfo(id, id.toUpper());
    }

    SingerInfo singer(const QString &id, const QList<SpeakerInfo> &speakers) {
        return SingerInfo({id, QStringLiteral("test-package"), QVersionNumber(1, 0)}, id.toUpper(),
                          speakers);
    }

    SpeakerMixModel::SpeakerMixData fixedMix(const SpeakerInfo &first, const SpeakerInfo &second,
                                             const double weight = 2.0) {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
        data.sources = {{first}, {second}};
        data.fixedWeights = {weight};
        data.sourcePresetId = QStringLiteral(" preset-%1 ").arg(weight);
        data.sourcePresetName = QStringLiteral(" Unicode 混合 %1 ").arg(weight);
        return data;
    }

    SpeakerMixModel::SpeakerMixData dynamicMix(const SpeakerInfo &first, const SpeakerInfo &second,
                                               const int laterTick = 960) {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
        data.sources = {{first}, {second}};
        data.dynamicKeyframes = {
            {laterTick, {1.0}},
            {0,         {0.0}}
        };
        return data;
    }

    class Fixture final {
    public:
        Fixture()
            : m_history(resetHistory()), m_runtime(&m_model, m_history),
              speakerA(speaker(QStringLiteral("speaker-a"))),
              speakerB(speaker(QStringLiteral("speaker-b"))),
              singerA(singer(QStringLiteral("singer-a"), {speakerA, speakerB})) {
            const auto firstTrack = m_runtime.project().insertTrack(
                commandContext(m_runtime), 0,
                trackDraft(QStringLiteral("第一轨 🚀"), QStringLiteral("fixture-track-a")));
            const auto secondTrack = m_runtime.project().insertTrack(
                commandContext(m_runtime), 1,
                trackDraft(QStringLiteral("Second"), QStringLiteral("fixture-track-b")));
            const auto thirdTrack = m_runtime.project().insertTrack(
                commandContext(m_runtime), 2,
                trackDraft(QStringLiteral("Third"), QStringLiteral("fixture-track-c")));
            if (firstTrack)
                trackA = TrackId(firstTrack.get().affectedObjects.first().value);
            if (secondTrack)
                trackB = TrackId(secondTrack.get().affectedObjects.first().value);
            if (thirdTrack)
                trackC = TrackId(thirdTrack.get().affectedObjects.first().value);

            auto firstClip =
                singingClipDraft(QStringLiteral("主唱片段 🎵"), QStringLiteral("fixture-clip-a"));
            auto secondClip =
                singingClipDraft(QStringLiteral("Second Clip"), QStringLiteral("fixture-clip-b"));
            secondClip.properties.start = 4800;
            auto thirdClip =
                singingClipDraft(QStringLiteral("Third Clip"), QStringLiteral("fixture-clip-c"));
            thirdClip.properties.start = 9600;
            const auto clips = m_runtime.project().insertClips(
                commandContext(m_runtime), {
                                               {.trackId = trackA, .clip = firstClip },
                                               {.trackId = trackA, .clip = secondClip},
                                               {.trackId = trackB, .clip = thirdClip }
            });
            if (clips && clips.get().affectedObjects.size() == 3) {
                clipA = ClipId(clips.get().affectedObjects.at(0).value);
                clipB = ClipId(clips.get().affectedObjects.at(1).value);
                clipC = ClipId(clips.get().affectedObjects.at(2).value);
            }

            auto firstNote =
                noteDraft(73, 407, 60, QStringLiteral("la"), QStringLiteral("fixture-note-a"));
            PhonemeName onset;
            onset.language = QStringLiteral("en");
            onset.name = QStringLiteral("l");
            onset.isOnset = true;
            PhonemeName vowel;
            vowel.language = QStringLiteral("en");
            vowel.name = QStringLiteral("a");
            firstNote.phonemes.nameSeq.edited = {onset, vowel};
            firstNote.phonemes.offsetSeq.original = {-40, 80};
            const auto notes = m_runtime.notes().insertNotes(
                commandContext(m_runtime), clipA,
                {firstNote,
                 noteDraft(600, 480, 64, QStringLiteral("mi"), QStringLiteral("fixture-note-b")),
                 noteDraft(1440, 240, 67, QStringLiteral("so"), QStringLiteral("fixture-note-c"))});
            if (notes && notes.get().affectedObjects.size() == 3) {
                noteA = NoteId(notes.get().affectedObjects.at(0).value);
                noteB = NoteId(notes.get().affectedObjects.at(1).value);
                noteC = NoteId(notes.get().affectedObjects.at(2).value);
            }

            m_runtime.timeline().setTempo(commandContext(m_runtime), 960, 135.0);
            m_runtime.timeline().setTempo(commandContext(m_runtime), 1920, 145.0);
            m_runtime.timeline().setTimeSignature(commandContext(m_runtime), 4, 3, 4);
            m_runtime.timeline().setTimeSignature(commandContext(m_runtime), 8, 6, 8);
            m_history->reset();
        }

        ~Fixture() {
            m_history->reset();
        }

        CoreRuntime &runtime() {
            return m_runtime;
        }

        HistoryManager *history() const {
            return m_history;
        }

        QByteArray stateFingerprint() {
            QByteArray result;
            const auto project =
                m_runtime.project().getProject(m_runtime.documentVersion().documentId);
            if (project) {
                for (const auto &track : project.get().tracks) {
                    auto draft = track.data;
                    draft.clips.clear();
                    result += QByteArray::number(track.id.value()) + ':';
                    for (const auto &clip : track.clips) {
                        result += QByteArray::number(clip.id.value()) + ':';
                        draft.clips.append(clip.data);
                    }
                    result += Automation::fingerprint(draft);
                }
            }
            const auto timeline =
                m_runtime.timeline().getTimeline(m_runtime.documentVersion().documentId);
            if (timeline) {
                for (const auto &tempo : timeline.get().tempos) {
                    result += QByteArray::number(tempo.pos) + '=' +
                              QByteArray::number(tempo.value, 'g', 17);
                }
                for (const auto &signature : timeline.get().timeSignatures) {
                    result += QByteArray::number(signature.barIndex) + '=' +
                              QByteArray::number(signature.numerator) + '/' +
                              QByteArray::number(signature.denominator);
                }
            }
            result +=
                QJsonDocument(m_model.masterControl().serialize()).toJson(QJsonDocument::Compact);
            return result;
        }

        std::optional<Automation::TrackSnapshotDto> track(const TrackId id) {
            const auto project =
                m_runtime.project().getProject(m_runtime.documentVersion().documentId);
            if (!project)
                return std::nullopt;
            for (const auto &snapshot : project.get().tracks) {
                if (snapshot.id == id)
                    return snapshot;
            }
            return std::nullopt;
        }

        std::optional<Automation::ClipSnapshotDto> clip(const ClipId id) {
            const auto project =
                m_runtime.project().getProject(m_runtime.documentVersion().documentId);
            if (!project)
                return std::nullopt;
            for (const auto &track : project.get().tracks) {
                for (const auto &snapshot : track.clips) {
                    if (snapshot.id == id)
                        return snapshot;
                }
            }
            return std::nullopt;
        }

        std::optional<Automation::NoteSnapshotDto> note(const NoteId id) {
            const auto notes =
                m_runtime.notes().getNotes(m_runtime.documentVersion().documentId, clipA);
            if (!notes)
                return std::nullopt;
            for (const auto &snapshot : notes.get()) {
                if (snapshot.id == id)
                    return snapshot;
            }
            return std::nullopt;
        }

        TrackId trackA;
        TrackId trackB;
        TrackId trackC;
        ClipId clipA;
        ClipId clipB;
        ClipId clipC;
        NoteId noteA;
        NoteId noteB;
        NoteId noteC;
        SpeakerInfo speakerA;
        SpeakerInfo speakerB;
        SingerInfo singerA;

    private:
        static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset();
            return history;
        }

        HistoryManager *m_history;
        AppModel m_model;
        CoreRuntime m_runtime;
    };

    using MutationCall =
        std::function<AutomationResult<MutationResult>(Fixture &, const CommandContext &, int)>;
    using Preparation = std::function<void(Fixture &)>;

    struct CommandSpec final {
        OperationId operationId;
        MutationCall valid;
        MutationCall missingObject;
        MutationCall invalidDomain;
        MutationCall noOp;
        Preparation prepare;
        Preparation prepareNoOp;
        bool recordsHistory = true;
        bool createsObjects = false;
        bool conflictByExpectedRevision = false;
    };

    Automation::CurveDraftDto curve(const int start, const int value) {
        Automation::CurveDraftDto draft;
        draft.type = Automation::CurveDraftDto::Type::Draw;
        draft.localStart = start;
        draft.step = 5;
        draft.values = {value, value + 10, value + 20};
        return draft;
    }

    TrackControl masterControl(const double gain, const double pan) {
        TrackControl control;
        control.setGain(gain);
        control.setPan(pan);
        control.setMute(gain < 0.0);
        return control;
    }

    QList<CommandSpec> highValueSpecs() {
        return {
            {
             .operationId = Automation::OperationIds::tracks::insert,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().insertTrack(
                            context, variant == 0 ? 1 : 2,
                            trackDraft(QStringLiteral("新增轨 %1 🧪").arg(variant),
                                       QStringLiteral("dimension-track-%1").arg(variant)));
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().insertTrack(
                            context, -1, trackDraft(QStringLiteral("invalid")));
                    }, .recordsHistory = true,
             .createsObjects = true,
             },
            {
             .operationId = Automation::OperationIds::tracks::move,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().moveTrack(
                            context, variant == 0 ? fixture.trackA : fixture.trackB,
                            variant == 0 ? 3 : 0);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().moveTrack(context, TrackId(999999), -1);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().moveTrack(context, fixture.trackA, -1);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().moveTrack(context, fixture.trackA, 0);
                    }, },
            {
             .operationId = Automation::OperationIds::tracks::remove,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().removeTracks(
                            context, {variant == 0 ? fixture.trackC : fixture.trackB});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().removeTracks(context, {TrackId(999999)});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().removeTracks(
                            context, {fixture.trackC, fixture.trackC});
                    }, .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().project().removeTracks(context, {}); },
             },
            {
             .operationId = Automation::OperationIds::tracks::set_color,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().setTrackColor(context, fixture.trackA,
                                                                         variant == 0 ? 3 : 4);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackColor(context, TrackId(999999),
                                                                         -1);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackColor(
                            context, fixture.trackA, AutomationWire::TrackPaletteColorCount);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackColor(context, fixture.trackA,
                                                                         2);
                    }, .recordsHistory = true,
             },
            {
             .operationId = Automation::OperationIds::tracks::set_default_language,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().setTrackDefaultLanguage(
                            context, fixture.trackA,
                            variant == 0 ? QStringLiteral("zh-Hans-测试")
                                         : QStringLiteral("ja-JP"));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackDefaultLanguage(
                            context, TrackId(999999), QString());
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackDefaultLanguage(
                            context, fixture.trackA, QStringLiteral("  "));
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackDefaultLanguage(
                            context, fixture.trackA, QStringLiteral("en"));
                    }, .recordsHistory = true,
             },
            {
             .operationId = Automation::OperationIds::tracks::set_properties,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().setTrackProperties(
                            context, {.id = fixture.trackA,
                                      .name = QStringLiteral("属性轨 %1 ✨").arg(variant),
                                      .gain = variant == 0 ? 0.75 : 0.5,
                                      .pan = variant == 0 ? 0.25 : -0.5,
                                      .mute = variant == 0,
                                      .solo = variant != 0});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackProperties(
                            context, {.id = TrackId(999999), .gain = std::nan(""), .pan = 0.0});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setTrackProperties(
                            context, {.id = fixture.trackA,
                                      .name = QStringLiteral("bad"),
                                      .gain = std::nan(""),
                                      .pan = 0.0});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        const auto current = fixture.track(fixture.trackA);
                        return fixture.runtime().project().setTrackProperties(
                            context, {.id = fixture.trackA,
                                      .name = current->data.name,
                                      .gain = current->data.gain,
                                      .pan = current->data.pan,
                                      .mute = current->data.mute,
                                      .solo = current->data.solo});
                    }, },
            {
             .operationId = Automation::OperationIds::notes::insert,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().insertNotes(
                            context, fixture.clipA,
                            {noteDraft(2100 + variant * 300, 240, 69 + variant,
                                       QStringLiteral("新音符 %1 🌟").arg(variant),
                                       QStringLiteral("dimension-note-%1").arg(variant))});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().insertNotes(
                            context, ClipId(999999),
                            {noteDraft(0, 0, -1, QStringLiteral("invalid"))});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().insertNotes(
                            context, fixture.clipA,
                            {noteDraft(0, 0, 60, QStringLiteral("invalid"))});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().insertNotes(context, fixture.clipA, {});
                    }, .createsObjects = true,
             },
            {
             .operationId = Automation::OperationIds::clips::insert,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        auto draft =
                            singingClipDraft(QStringLiteral("新增片段 %1 🎶").arg(variant),
                                             QStringLiteral("dimension-clip-%1").arg(variant));
                        draft.properties.start = 14000 + variant * 4000;
                        return fixture.runtime().project().insertClips(
                            context, {
                                         {.trackId = variant == 0 ? fixture.trackA : fixture.trackB,
             .clip = draft}});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        auto draft = singingClipDraft(QStringLiteral("invalid"));
                        draft.properties.length = -1;
                        return fixture.runtime().project().insertClips(
                            context, {
                                         {.trackId = TrackId(999999), .clip = draft}});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        auto draft = singingClipDraft(QStringLiteral("invalid"));
                        draft.properties.length = -1;
                        return fixture.runtime().project().insertClips(
                            context, {
                                         {.trackId = fixture.trackA, .clip = draft}});
                    }, .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().project().insertClips(context, {}); },
             .createsObjects = true,
             },
            {
             .operationId = Automation::OperationIds::clips::remove,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().removeClips(
                            context, {variant == 0 ? fixture.clipB : fixture.clipC});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().removeClips(context, {ClipId(999999)});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().removeClips(
                            context, {fixture.clipB, fixture.clipB});
                    }, .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().project().removeClips(context, {}); },
             },
            {
             .operationId = Automation::OperationIds::clips::set_default_language,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().project().setSingingClipDefaultLanguage(
                            context, fixture.clipA,
                            variant == 0 ? QStringLiteral("yue-Hant-粵") : QStringLiteral("ko-KR"));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setSingingClipDefaultLanguage(
                            context, ClipId(999999), QString());
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setSingingClipDefaultLanguage(
                            context, fixture.clipA, QString());
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setSingingClipDefaultLanguage(
                            context, fixture.clipA, QStringLiteral("en"));
                    }, .recordsHistory = true,
             },
            {
             .operationId = Automation::OperationIds::clips::set_properties,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        const auto snapshot = fixture.clip(fixture.clipA);
                        auto properties = snapshot->data.properties;
                        properties.id = fixture.clipA;
                        properties.name = QStringLiteral("片段属性 %1 🎼").arg(variant);
                        properties.start = variant == 0 ? 240 : 480;
                        properties.gain = variant == 0 ? 0.75 : 0.5;
                        properties.mute = variant != 0;
                        return fixture.runtime().project().setClipProperties(
                            context, properties, variant == 0 ? fixture.trackA : fixture.trackB);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().project().setClipProperties(
                            context, {.id = ClipId(999999), .gain = std::nan("")}, TrackId(999999));
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        const auto snapshot = fixture.clip(fixture.clipA);
                        auto properties = snapshot->data.properties;
                        properties.id = fixture.clipA;
                        properties.gain = std::nan("");
                        return fixture.runtime().project().setClipProperties(context, properties,
                                                                             fixture.trackA);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        const auto snapshot = fixture.clip(fixture.clipA);
                        auto properties = snapshot->data.properties;
                        properties.id = fixture.clipA;
                        return fixture.runtime().project().setClipProperties(context, properties,
                                                                             fixture.trackA);
                    }, },
            {
             .operationId = Automation::OperationIds::notes::move,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().moveNotes(
                            context, fixture.clipA, {variant == 0 ? fixture.noteA : fixture.noteB},
                            120, 1);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().moveNotes(context, ClipId(999999),
                                                                   {NoteId(999999)}, 0, 100);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().moveNotes(context, fixture.clipA,
                                                                   {fixture.noteA}, 0, 100);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().moveNotes(context, fixture.clipA,
                                                                   {fixture.noteA}, 0, 0);
                    }, },
            {
             .operationId = Automation::OperationIds::parameters::replace,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().replaceParameter(
                            context, fixture.clipA, ParamInfo::Pitch, Param::Edited,
                            {curve(20 + variant * 10, 6000 + variant * 100)});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceParameter(
                            context, ClipId(999999), ParamInfo::Unknown, Param::Unknown, {});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceParameter(
                            context, fixture.clipA, ParamInfo::Unknown, Param::Unknown, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceParameter(
                            context, fixture.clipA, ParamInfo::Pitch, Param::Edited, {});
                    }, },
            {
             .operationId = Automation::OperationIds::clips::set_voice,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().selectClipSingleSpeaker(
                            context, fixture.clipA, fixture.singerA,
                            variant == 0 ? fixture.speakerA : fixture.speakerB);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().selectClipSingleSpeaker(
                            context, ClipId(999999), {}, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().selectClipSingleSpeaker(
                            context, fixture.clipA, fixture.singerA, fixture.speakerA);
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectClipSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA);
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::tempos::set,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().timeline().setTempo(context, 2880,
                                                                     variant == 0 ? 155.0 : 165.0);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setTempo(
                            context, -1, std::numeric_limits<double>::quiet_NaN());
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setTempo(context, 960, 135.0);
                    }, },
            {
             .operationId = Automation::OperationIds::master::set_control,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().timeline().setMasterControl(
                            context,
                            masterControl(variant == 0 ? 0.75 : -0.25, variant == 0 ? 0.25 : -0.5));
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setMasterControl(
                            context, masterControl(std::numeric_limits<double>::quiet_NaN(), 0.0));
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setMasterControl(context,
                                                                             TrackControl{});
                    }, },
            {
             .operationId = Automation::OperationIds::history::undo,
             .valid = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().history().undo(context); },
             .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().history().undo(context); },
             .prepare =
                    [](Fixture &fixture) {
                        fixture.runtime().timeline().setTempo(commandContext(fixture.runtime()),
                                                              3000, 151.0);
                    }, .recordsHistory = false,
             .conflictByExpectedRevision = true,
             },
            {
             .operationId = Automation::OperationIds::notes::quantize,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().quantizeNotes(
                            context, fixture.clipA,
                            variant == 0 ? QList<NoteId>{fixture.noteA}
                                         : QList<NoteId>{fixture.noteA, fixture.noteB},
                            16, true, true);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().quantizeNotes(
                            context, ClipId(999999), {NoteId(999999)}, 0, true, true);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().quantizeNotes(
                            context, fixture.clipA, {fixture.noteA}, 0, true, true);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().quantizeNotes(
                            context, fixture.clipA, {fixture.noteC}, 16, true, true);
                    }, },
            {
             .operationId = Automation::OperationIds::notes::remove,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().removeNotes(
                            context, fixture.clipA, {variant == 0 ? fixture.noteC : fixture.noteB});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().removeNotes(context, ClipId(999999),
                                                                     {NoteId(999999)});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().removeNotes(
                            context, fixture.clipA, {fixture.noteC, fixture.noteC});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().removeNotes(context, fixture.clipA, {});
                    }, },
            {
             .operationId = Automation::OperationIds::notes::resize_left,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().resizeNotesLeft(
                            context, fixture.clipA, {variant == 0 ? fixture.noteA : fixture.noteB},
                            20, 1);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesLeft(context, ClipId(999999),
                                                                         {NoteId(999999)}, 10, 0);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesLeft(context, fixture.clipA,
                                                                         {fixture.noteA}, 10, 0);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesLeft(context, fixture.clipA, {},
                                                                         20, 1);
                    }, },
            {
             .operationId = Automation::OperationIds::notes::resize_right,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().resizeNotesRight(
                            context, fixture.clipA, {variant == 0 ? fixture.noteA : fixture.noteB},
                            40, 1);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesRight(context, ClipId(999999),
                                                                          {NoteId(999999)}, 10, 0);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesRight(context, fixture.clipA,
                                                                          {fixture.noteA}, 10, 0);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().resizeNotesRight(context, fixture.clipA,
                                                                          {}, 20, 1);
                    }, },
            {
             .operationId = Automation::OperationIds::notes::set_phoneme_offsets,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().notes().setPhonemeOffsets(
                            context, fixture.clipA, fixture.noteA,
                            variant == 0 ? QList<int>{-30, 90} : QList<int>{-20, 100});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setPhonemeOffsets(
                            context, ClipId(999999), NoteId(999999), {100, -100});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setPhonemeOffsets(
                            context, fixture.clipA, fixture.noteA, {100, -100});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setPhonemeOffsets(context, fixture.clipA,
                                                                           fixture.noteA, {});
                    }, },
            {
             .operationId = Automation::OperationIds::notes::set_word_properties,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        Automation::NoteWordEditDto edit;
                        edit.noteId = fixture.noteA;
                        edit.lyric = variant == 0 ? QStringLiteral("  你好 🌍  ")
                                                  : QStringLiteral("  再见 🌙  ");
                        edit.language = variant == 0 ? QStringLiteral("zh") : QStringLiteral("yue");
                        return fixture.runtime().notes().setWordProperties(context, fixture.clipA,
                                                                           {edit});
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setWordProperties(
                            context, ClipId(999999),
                            {{.noteId = NoteId(999999)}, {.noteId = NoteId(999999)}});
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setWordProperties(
                            context, fixture.clipA,
                            {{.noteId = fixture.noteA}, {.noteId = fixture.noteA}});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().setWordProperties(context, fixture.clipA,
                                                                           {});
                    }, },
            {
             .operationId = Automation::OperationIds::notes::split,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        const int firstLength = variant == 0 ? 180 : 240;
                        auto child =
                            noteDraft(600 + firstLength, 480 - firstLength, 64,
                                      variant == 0 ? QStringLiteral("+") : QStringLiteral("-"),
                                      QStringLiteral("dimension-split-%1").arg(variant));
                        return fixture.runtime().notes().splitNote(
                            context, fixture.clipA, fixture.noteB, child, firstLength);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().splitNote(context, ClipId(999999),
                                                                   NoteId(999999), {}, 0);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().notes().splitNote(context, fixture.clipA,
                                                                   fixture.noteB, {}, 0);
                    }, .createsObjects = true,
             },
            {
             .operationId = Automation::OperationIds::history::redo,
             .valid = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().history().redo(context); },
             .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().history().redo(context); },
             .prepare =
                    [](Fixture &fixture) {
                        fixture.runtime().timeline().setTempo(commandContext(fixture.runtime()),
                                                              3000, 151.0);
                        fixture.runtime().history().undo(commandContext(fixture.runtime()));
                    }, .recordsHistory = false,
             .conflictByExpectedRevision = true,
             },
        };
    }

    QList<CommandSpec> remainingSpecs() {
        return {
            {
             .operationId = Automation::OperationIds::speaker_mix::clip::apply,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().applyClipSpeakerMix(
                            context, fixture.clipA, fixture.singerA,
                            variant == 0 ? fixture.speakerA : fixture.speakerB,
                            fixedMix(fixture.speakerA, fixture.speakerB, variant == 0 ? 2.0 : 3.0));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().applyClipSpeakerMix(
                            context, ClipId(999999), {}, {}, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().applyClipSpeakerMix(
                            context, fixture.clipA, fixture.singerA, fixture.speakerA,
                            fixedMix(fixture.speakerA, fixture.speakerB));
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().applyClipSpeakerMix(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA, fixedMix(fixture.speakerA, fixture.speakerB));
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::speaker_mix::clip::enable_dynamic,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().enableClipDynamicSpeakerMix(
                            context, fixture.clipA, fixture.singerA,
                            variant == 0 ? fixture.speakerA : fixture.speakerB,
                            dynamicMix(fixture.speakerA, fixture.speakerB,
                                       variant == 0 ? 960 : 1440));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().enableClipDynamicSpeakerMix(
                            context, ClipId(999999), {}, {}, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().enableClipDynamicSpeakerMix(
                            context, fixture.clipA, fixture.singerA, fixture.speakerA,
                            dynamicMix(fixture.speakerA, fixture.speakerB));
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().enableClipDynamicSpeakerMix(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA, dynamicMix(fixture.speakerA, fixture.speakerB));
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::speaker_mix::clip::replace,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().replaceClipSpeakerMix(
                            context, fixture.clipA,
                            variant == 0 ? fixedMix(fixture.speakerA, fixture.speakerB)
                                         : dynamicMix(fixture.speakerA, fixture.speakerB, 1440));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceClipSpeakerMix(
                            context, ClipId(999999), {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceClipSpeakerMix(
                            context, fixture.clipA, fixedMix(fixture.speakerA, fixture.speakerB));
                    }, .prepare =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectClipSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA);
                        fixture.history()->reset();
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectClipSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA);
                        fixture.runtime().parameters().replaceClipSpeakerMix(
                            commandContext(fixture.runtime()), fixture.clipA,
                            fixedMix(fixture.speakerA, fixture.speakerB));
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::clips::use_track_voice,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().useTrackVoiceContext(context,
                                                                                   fixture.clipA);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().useTrackVoiceContext(context,
                                                                                   ClipId(999999));
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().useTrackVoiceContext(context,
                                                                                   fixture.clipA);
                    }, .prepare =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectClipSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.clipA, fixture.singerA,
                            fixture.speakerA);
                        fixture.history()->reset();
                    }, .conflictByExpectedRevision = true,
             },
            {
             .operationId = Automation::OperationIds::speaker_mix::track::apply,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().applyTrackSpeakerMix(
                            context, fixture.trackA, fixture.singerA,
                            variant == 0 ? fixture.speakerA : fixture.speakerB,
                            fixedMix(fixture.speakerA, fixture.speakerB, variant == 0 ? 2.0 : 4.0));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().applyTrackSpeakerMix(
                            context, TrackId(999999), {}, {}, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().applyTrackSpeakerMix(
                            context, fixture.trackA, fixture.singerA, fixture.speakerA,
                            fixedMix(fixture.speakerA, fixture.speakerB));
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().applyTrackSpeakerMix(
                            commandContext(fixture.runtime()), fixture.trackA, fixture.singerA,
                            fixture.speakerA, fixedMix(fixture.speakerA, fixture.speakerB));
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::speaker_mix::track::replace,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().replaceTrackSpeakerMix(
                            context, fixture.trackA,
                            variant == 0 ? fixedMix(fixture.speakerA, fixture.speakerB)
                                         : dynamicMix(fixture.speakerA, fixture.speakerB, 1440));
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceTrackSpeakerMix(
                            context, TrackId(999999), {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().replaceTrackSpeakerMix(
                            context, fixture.trackA, fixedMix(fixture.speakerA, fixture.speakerB));
                    }, .prepare =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectTrackSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.trackA, fixture.singerA,
                            fixture.speakerA);
                        fixture.history()->reset();
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectTrackSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.trackA, fixture.singerA,
                            fixture.speakerA);
                        fixture.runtime().parameters().replaceTrackSpeakerMix(
                            commandContext(fixture.runtime()), fixture.trackA,
                            fixedMix(fixture.speakerA, fixture.speakerB));
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::tracks::set_voice,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().parameters().selectTrackSingleSpeaker(
                            context, fixture.trackA, fixture.singerA,
                            variant == 0 ? fixture.speakerA : fixture.speakerB);
                    }, .missingObject =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().selectTrackSingleSpeaker(
                            context, TrackId(999999), {}, {});
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().parameters().selectTrackSingleSpeaker(
                            context, fixture.trackA, fixture.singerA, fixture.speakerA);
                    }, .prepareNoOp =
                    [](Fixture &fixture) {
                        fixture.runtime().parameters().selectTrackSingleSpeaker(
                            commandContext(fixture.runtime()), fixture.trackA, fixture.singerA,
                            fixture.speakerA);
                        fixture.history()->reset();
                    }, },
            {
             .operationId = Automation::OperationIds::tempos::remove,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().timeline().deleteTempo(context,
                                                                        variant == 0 ? 960 : 1920);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().deleteTempo(context, 0);
                    }, .noOp = [](Fixture &fixture, const CommandContext &context,
             int) { return fixture.runtime().timeline().deleteTempo(context, 7777); },
             },
            {
             .operationId = Automation::OperationIds::time_signatures::set,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().timeline().setTimeSignature(
                            context, 12, variant == 0 ? 5 : 7, variant == 0 ? 4 : 8);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setTimeSignature(context, -1, 0, 3);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().setTimeSignature(context, 4, 3, 4);
                    }, },
            {
             .operationId = Automation::OperationIds::time_signatures::remove,
             .valid =
                    [](Fixture &fixture, const CommandContext &context, const int variant) {
                        return fixture.runtime().timeline().deleteTimeSignature(
                            context, variant == 0 ? 4 : 8);
                    }, .invalidDomain =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().deleteTimeSignature(context, 0);
                    }, .noOp =
                    [](Fixture &fixture, const CommandContext &context, int) {
                        return fixture.runtime().timeline().deleteTimeSignature(context, 777);
                    }, },
        };
    }

    bool isError(const AutomationResult<MutationResult> &result, const AutomationErrorCode code,
                 const OperationId &operationId) {
        return !result && result.getError().code == code &&
               result.getError().operationId == operationId;
    }

    void runHighValueCommandDimensions(Suite &suite, const QList<CommandSpec> &specs) {
        for (const auto &spec : specs) {
            suite.run(spec.operationId, QStringLiteral("C1-COMMIT"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto result = spec.valid(fixture, commandContext(fixture.runtime()), 0);
                suite.expect(result && result.get().changed && !result.get().validatedOnly,
                             QStringLiteral("real handler must commit a change"));
                suite.expect(result && result.get().previous == beforeVersion &&
                                 result.get().current == fixture.runtime().documentVersion() &&
                                 result.get().current.revision == beforeVersion.revision + 1,
                             QStringLiteral("commit must advance exactly one revision"));
                suite.expect(fixture.stateFingerprint() != beforeState,
                             QStringLiteral("commit must change observable document state"));
                if (spec.createsObjects) {
                    suite.expect(result && result.get().createdObjects.size() == 1 &&
                                     result.get().affectedObjects.contains(
                                         result.get().createdObjects.first().object),
                                 QStringLiteral("create handler must return its stable binding"));
                }
            });

            if (spec.noOp) {
                suite.run(spec.operationId, QStringLiteral("C2-NOOP-SIDE-EFFECTS"), [&] {
                    Fixture fixture;
                    if (spec.prepareNoOp)
                        spec.prepareNoOp(fixture);
                    const auto beforeVersion = fixture.runtime().documentVersion();
                    const auto beforeState = fixture.stateFingerprint();
                    const auto historyBefore =
                        fixture.runtime().history().getState(beforeVersion.documentId);
                    const auto result = spec.noOp(fixture, commandContext(fixture.runtime()), 0);
                    const auto historyAfter = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    suite.expect(result && !result.get().changed && !result.get().validatedOnly,
                                 QStringLiteral("legal no-op must report changed=false"));
                    suite.expect(
                        fixture.runtime().documentVersion() == beforeVersion &&
                            fixture.stateFingerprint() == beforeState && historyBefore &&
                            historyAfter &&
                            historyBefore.get().canUndo == historyAfter.get().canUndo &&
                            historyBefore.get().canRedo == historyAfter.get().canRedo,
                        QStringLiteral("no-op must preserve model, revision, and History"));
                });
            }

            suite.run(spec.operationId, QStringLiteral("C3-VALIDATE-KEY-RELEASE"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                const auto key = QStringLiteral("editdim-preview-%1").arg(spec.operationId);
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto historyBefore =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                const auto preview =
                    spec.valid(fixture, commandContext(fixture.runtime(), true, key), 0);
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 preview.get().current.revision == beforeVersion.revision + 1,
                             QStringLiteral("validate-only must return a predicted mutation"));
                suite.expect(preview &&
                                 (!spec.createsObjects || preview.get().createdObjects.isEmpty()) &&
                                 fixture.runtime().documentVersion() == beforeVersion &&
                                 fixture.stateFingerprint() == beforeState,
                             QStringLiteral("validate-only must not mutate or allocate IDs"));
                const auto historyAfter = fixture.runtime().history().getState(
                    fixture.runtime().documentVersion().documentId);
                suite.expect(historyBefore && historyAfter &&
                                 historyBefore.get().canUndo == historyAfter.get().canUndo &&
                                 historyBefore.get().canRedo == historyAfter.get().canRedo,
                             QStringLiteral("validate-only must not touch History"));
                const auto committed =
                    spec.valid(fixture, commandContext(fixture.runtime(), false, key), 1);
                suite.expect(committed && committed.get().changed,
                             QStringLiteral("validate-only must not claim its idempotency key"));
            });

            suite.run(spec.operationId, QStringLiteral("C5-ERROR-PRIORITY"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                auto wrongDocument = commandContext(fixture.runtime());
                wrongDocument.expected.documentId = Automation::DocumentId::create();
                wrongDocument.expected.revision += 100;
                const auto invalidCall =
                    spec.missingObject ? spec.missingObject
                                       : (spec.invalidDomain ? spec.invalidDomain : spec.valid);
                const auto documentResult = invalidCall(fixture, wrongDocument, 0);
                suite.expect(
                    isError(documentResult, AutomationErrorCode::DocumentChanged, spec.operationId),
                    QStringLiteral("DocumentId must precede all lower validation layers"));

                auto stale = commandContext(fixture.runtime());
                stale.expected.revision += 100;
                const auto revisionResult = invalidCall(fixture, stale, 0);
                suite.expect(isError(revisionResult, AutomationErrorCode::RevisionConflict,
                                     spec.operationId),
                             QStringLiteral("revision must precede object/domain validation"));

                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                if (spec.missingObject) {
                    const auto objectResult =
                        spec.missingObject(fixture, commandContext(fixture.runtime()), 0);
                    suite.expect(
                        isError(objectResult, AutomationErrorCode::NotFound, spec.operationId),
                        QStringLiteral("missing object must be reported before domain data"));
                }
                if (spec.invalidDomain) {
                    const auto domainResult =
                        spec.invalidDomain(fixture, commandContext(fixture.runtime()), 0);
                    suite.expect(
                        isError(domainResult, AutomationErrorCode::InvalidArgument,
                                spec.operationId),
                        QStringLiteral("valid target with invalid domain data must be rejected"));
                }
                suite.expect(fixture.runtime().documentVersion() == beforeVersion &&
                                 fixture.stateFingerprint() == beforeState,
                             QStringLiteral("all rejected layers must be side-effect free"));
            });

            suite.run(spec.operationId, QStringLiteral("C6-HISTORY-REVISION-UNDO-REDO"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto result = spec.valid(fixture, commandContext(fixture.runtime()), 1);
                const auto committedVersion = fixture.runtime().documentVersion();
                const auto committedState = fixture.stateFingerprint();
                suite.expect(result && result.get().changed &&
                                 committedVersion.revision == beforeVersion.revision + 1,
                             QStringLiteral("History dimension must commit exactly one revision"));

                if (spec.operationId == Automation::OperationIds::history::undo) {
                    const auto state = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    const auto restore =
                        fixture.runtime().history().redo(commandContext(fixture.runtime()));
                    suite.expect(
                        state && state.get().canRedo && restore && restore.get().changed &&
                            fixture.stateFingerprint() == beforeState &&
                            fixture.runtime().documentVersion().revision ==
                                beforeVersion.revision + 2,
                        QStringLiteral("undo must expose one redo and restore deterministically"));
                } else if (spec.operationId == Automation::OperationIds::history::redo) {
                    const auto state = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    const auto restore =
                        fixture.runtime().history().undo(commandContext(fixture.runtime()));
                    suite.expect(
                        state && state.get().canUndo && restore && restore.get().changed &&
                            fixture.stateFingerprint() == beforeState &&
                            fixture.runtime().documentVersion().revision ==
                                beforeVersion.revision + 2,
                        QStringLiteral("redo must expose one undo and restore deterministically"));
                } else if (spec.recordsHistory) {
                    const auto afterCommit = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    const auto undo =
                        fixture.runtime().history().undo(commandContext(fixture.runtime()));
                    const auto afterUndo = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    const auto undoState = fixture.stateFingerprint();
                    const auto redo =
                        fixture.runtime().history().redo(commandContext(fixture.runtime()));
                    suite.expect(
                        afterCommit && afterCommit.get().canUndo && undo && undo.get().changed &&
                            afterUndo && !afterUndo.get().canUndo && afterUndo.get().canRedo &&
                            undoState == beforeState,
                        QStringLiteral("recorded command must create exactly one undo entry"));
                    suite.expect(
                        redo && redo.get().changed &&
                            fixture.stateFingerprint() == committedState &&
                            fixture.runtime().documentVersion().revision ==
                                beforeVersion.revision + 3,
                        QStringLiteral("redo must restore state and advance one revision"));
                } else {
                    const auto history = fixture.runtime().history().getState(
                        fixture.runtime().documentVersion().documentId);
                    const auto undo =
                        fixture.runtime().history().undo(commandContext(fixture.runtime()));
                    suite.expect(
                        history && !history.get().canUndo && undo && !undo.get().changed &&
                            fixture.stateFingerprint() == committedState &&
                            fixture.runtime().documentVersion() == committedVersion,
                        QStringLiteral("non-History command must not create an undo entry"));
                }
            });

            suite.run(spec.operationId, QStringLiteral("C8-IDEMPOTENT-REPLAY"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                const auto context =
                    commandContext(fixture.runtime(), false,
                                   QStringLiteral("editdim-replay-%1").arg(spec.operationId));
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto first = spec.valid(fixture, context, 0);
                const auto afterFirstState = fixture.stateFingerprint();
                const auto replay = spec.valid(fixture, context, 0);
                QString replayDetail;
                if (!first) {
                    replayDetail = QStringLiteral("first error=%1")
                                       .arg(static_cast<int>(first.getError().code));
                } else if (!replay) {
                    replayDetail = QStringLiteral("replay error=%1")
                                       .arg(static_cast<int>(replay.getError().code));
                } else if (first.get() != replay.get()) {
                    replayDetail =
                        QStringLiteral("result mismatch changed=%1/%2 affected=%3/%4 created=%5/%6")
                            .arg(first.get().changed)
                            .arg(replay.get().changed)
                            .arg(first.get().affectedObjects.size())
                            .arg(replay.get().affectedObjects.size())
                            .arg(first.get().createdObjects.size())
                            .arg(replay.get().createdObjects.size());
                } else {
                    replayDetail = QStringLiteral("exact replay");
                }
                suite.expect(
                    first && replay && first.get() == replay.get(),
                    QStringLiteral("real Facade replay must return the original result (%1)")
                        .arg(replayDetail));
                suite.expect(fixture.runtime().documentVersion().revision ==
                                     beforeVersion.revision + 1 &&
                                 fixture.stateFingerprint() == afterFirstState,
                             QStringLiteral("replay must not execute handler side effects twice"));
            });

            suite.run(spec.operationId, QStringLiteral("C8-IDEMPOTENCY-CONFLICT"), [&] {
                Fixture fixture;
                if (spec.prepare)
                    spec.prepare(fixture);
                const auto key = QStringLiteral("editdim-conflict-%1").arg(spec.operationId);
                const auto originalContext = commandContext(fixture.runtime(), false, key);
                const auto first = spec.valid(fixture, originalContext, 0);
                const auto afterFirstVersion = fixture.runtime().documentVersion();
                const auto afterFirstState = fixture.stateFingerprint();
                auto conflictContext = originalContext;
                if (spec.conflictByExpectedRevision)
                    conflictContext.expected = afterFirstVersion;
                const auto conflict = spec.valid(fixture, conflictContext, 1);
                QString conflictDetail;
                if (!first) {
                    conflictDetail = QStringLiteral("first error=%1")
                                         .arg(static_cast<int>(first.getError().code));
                } else if (conflict) {
                    conflictDetail =
                        QStringLiteral(
                            "second request unexpectedly succeeded; replay=%1 noteB=%2 noteC=%3")
                            .arg(first.get() == conflict.get())
                            .arg(fixture.noteB.value())
                            .arg(fixture.noteC.value());
                } else {
                    conflictDetail =
                        QStringLiteral("code=%1 operation=%2 field=%3")
                            .arg(static_cast<int>(conflict.getError().code))
                            .arg(conflict.getError().operationId, conflict.getError().fieldPath);
                }
                suite.expect(
                    first &&
                        isError(conflict, AutomationErrorCode::IdempotencyConflict,
                                spec.operationId) &&
                        conflict.getError().fieldPath == QStringLiteral("idempotency_key"),
                    QStringLiteral("same key with different canonical request must conflict (%1)")
                        .arg(conflictDetail));
                suite.expect(fixture.runtime().documentVersion() == afterFirstVersion &&
                                 fixture.stateFingerprint() == afterFirstState,
                             QStringLiteral("idempotency conflict must not re-enter the handler"));
            });
        }
    }

    void runProjectQueryComplements(Suite &suite) {
        suite.run(Automation::OperationIds::project::get, QStringLiteral("Q1-MINIMUM-DTO"), [&] {
            AutomationTestSupport::TestRuntime fixture;
            auto &runtime = fixture.runtime();
            const auto result = runtime.project().getProject(runtime.documentVersion().documentId);
            suite.expect(
                result && result.get().document == runtime.documentVersion() &&
                    result.get().tracks.isEmpty(),
                QStringLiteral("minimum project query must return an empty typed snapshot"));
        });
        suite.run(Automation::OperationIds::project::get, QStringLiteral("Q1-POPULATED-DTO"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().project().getProject(
                fixture.runtime().documentVersion().documentId);
            suite.expect(
                result && result.get().tracks.size() == 3 &&
                    result.get().tracks.at(0).id == fixture.trackA &&
                    result.get().tracks.at(0).clips.size() == 2 &&
                    result.get().tracks.at(1).id == fixture.trackB,
                QStringLiteral("populated project snapshot must preserve typed hierarchy"));
        });
        suite.run(
            Automation::OperationIds::project::get, QStringLiteral("Q2-DETACHED-SNAPSHOT"), [&] {
                Fixture fixture;
                const auto snapshot = fixture.runtime().project().getProject(
                    fixture.runtime().documentVersion().documentId);
                const auto oldName = snapshot.get().tracks.first().data.name;
                const auto current = fixture.track(fixture.trackA);
                const auto changed = fixture.runtime().project().setTrackProperties(
                    commandContext(fixture.runtime()),
                    {.id = fixture.trackA,
                     .name = QStringLiteral("mutated after snapshot"),
                     .gain = current->data.gain,
                     .pan = current->data.pan,
                     .mute = current->data.mute,
                     .solo = current->data.solo});
                suite.expect(
                    snapshot && changed && snapshot.get().tracks.first().data.name == oldName &&
                        oldName != QStringLiteral("mutated after snapshot"),
                    QStringLiteral("project DTO must remain detached after later mutation"));
            });
        suite.run(Automation::OperationIds::project::get, QStringLiteral("Q4-UNICODE-ORDER"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().project().getProject(
                fixture.runtime().documentVersion().documentId);
            suite.expect(result &&
                             result.get().tracks.first().data.name == QStringLiteral("第一轨 🚀") &&
                             result.get().tracks.first().clips.first().data.properties.name ==
                                 QStringLiteral("主唱片段 🎵") &&
                             result.get().tracks.last().id == fixture.trackC,
                         QStringLiteral("query must preserve Unicode and stable insertion order"));
        });
        suite.run(
            Automation::OperationIds::project::get, QStringLiteral("Q5-NO-SIDE-EFFECTS"), [&] {
                Fixture fixture;
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto beforeHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                const auto result =
                    fixture.runtime().project().getProject(beforeVersion.documentId);
                const auto afterHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                suite.expect(
                    result && fixture.runtime().documentVersion() == beforeVersion &&
                        fixture.stateFingerprint() == beforeState && beforeHistory &&
                        afterHistory && beforeHistory.get().canUndo == afterHistory.get().canUndo,
                    QStringLiteral("project query must not change model, revision, or History"));
            });
    }

    void runNotesQueryComplements(Suite &suite) {
        suite.run(Automation::OperationIds::notes::list, QStringLiteral("Q1-MINIMUM-DTO"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().notes().getNotes(
                fixture.runtime().documentVersion().documentId, fixture.clipB);
            suite.expect(
                result && result.get().isEmpty(),
                QStringLiteral("notes query must represent an empty clip as an empty list"));
        });
        suite.run(Automation::OperationIds::notes::list, QStringLiteral("Q1-POPULATED-DTO"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().notes().getNotes(
                fixture.runtime().documentVersion().documentId, fixture.clipA);
            suite.expect(result && result.get().size() == 3 &&
                             result.get().at(0).id == fixture.noteA &&
                             result.get().at(1).id == fixture.noteB &&
                             result.get().at(2).id == fixture.noteC &&
                             result.get().first().clipId == fixture.clipA,
                         QStringLiteral("notes snapshot must preserve typed ownership and order"));
        });
        suite.run(
            Automation::OperationIds::notes::list, QStringLiteral("Q2-DETACHED-SNAPSHOT"), [&] {
                Fixture fixture;
                const auto snapshot = fixture.runtime().notes().getNotes(
                    fixture.runtime().documentVersion().documentId, fixture.clipA);
                Automation::NoteWordEditDto edit;
                edit.noteId = fixture.noteA;
                edit.lyric = QStringLiteral("后来改变 🌙");
                edit.language = QStringLiteral("zh");
                const auto changed = fixture.runtime().notes().setWordProperties(
                    commandContext(fixture.runtime()), fixture.clipA, {edit});
                suite.expect(snapshot && changed &&
                                 snapshot.get().first().data.lyric == QStringLiteral("la"),
                             QStringLiteral("note DTO must not alias a subsequently edited Note"));
            });
        suite.run(
            Automation::OperationIds::notes::list, QStringLiteral("Q4-UNICODE-LONG-TEXT"), [&] {
                Fixture fixture;
                const auto text = QStringLiteral("多语言歌词 🎤 — ") + QString(512, QChar(u'界'));
                Automation::NoteWordEditDto edit;
                edit.noteId = fixture.noteA;
                edit.lyric = text;
                edit.language = QStringLiteral("zh-Hant-x-測試");
                const auto changed = fixture.runtime().notes().setWordProperties(
                    commandContext(fixture.runtime()), fixture.clipA, {edit});
                const auto result = fixture.runtime().notes().getNotes(
                    fixture.runtime().documentVersion().documentId, fixture.clipA);
                suite.expect(changed && result && result.get().first().data.lyric == text &&
                                 result.get().first().data.language ==
                                     QStringLiteral("zh-Hant-x-測試"),
                             QStringLiteral("note query must round-trip Unicode and long text"));
            });
        suite.run(Automation::OperationIds::notes::list, QStringLiteral("Q5-NO-SIDE-EFFECTS"), [&] {
            Fixture fixture;
            const auto beforeVersion = fixture.runtime().documentVersion();
            const auto beforeState = fixture.stateFingerprint();
            const auto beforeHistory =
                fixture.runtime().history().getState(beforeVersion.documentId);
            const auto result =
                fixture.runtime().notes().getNotes(beforeVersion.documentId, fixture.clipA);
            const auto afterHistory =
                fixture.runtime().history().getState(beforeVersion.documentId);
            suite.expect(result && fixture.runtime().documentVersion() == beforeVersion &&
                             fixture.stateFingerprint() == beforeState && beforeHistory &&
                             afterHistory &&
                             beforeHistory.get().canUndo == afterHistory.get().canUndo,
                         QStringLiteral("notes query must not change model, revision, or History"));
        });
    }

    void runParameterQueryComplements(Suite &suite) {
        suite.run(Automation::OperationIds::parameters::get, QStringLiteral("Q1-MINIMUM-DTO"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().parameters().getParameter(
                fixture.runtime().documentVersion().documentId, fixture.clipA, ParamInfo::Pitch,
                Param::Edited);
            suite.expect(result && result.get().document == fixture.runtime().documentVersion() &&
                             result.get().clipId == fixture.clipA && result.get().curves.isEmpty(),
                         QStringLiteral("empty parameter must be a complete typed snapshot"));
        });
        suite.run(
            Automation::OperationIds::parameters::get, QStringLiteral("Q1-POPULATED-DTO"), [&] {
                Fixture fixture;
                const auto changed = fixture.runtime().parameters().replaceParameter(
                    commandContext(fixture.runtime()), fixture.clipA, ParamInfo::Pitch,
                    Param::Edited, {curve(20, 6100), curve(40, 6200)});
                const auto result = fixture.runtime().parameters().getParameter(
                    fixture.runtime().documentVersion().documentId, fixture.clipA, ParamInfo::Pitch,
                    Param::Edited);
                suite.expect(
                    changed && result && result.get().name == ParamInfo::Pitch &&
                        result.get().type == Param::Edited && result.get().curves.size() == 2 &&
                        result.get().curves.first().values == QList<int>({6100, 6110, 6120}),
                    QStringLiteral("populated parameter DTO must preserve all curve values"));
            });
        suite.run(
            Automation::OperationIds::parameters::get, QStringLiteral("Q2-DETACHED-SNAPSHOT"), [&] {
                Fixture fixture;
                fixture.runtime().parameters().replaceParameter(commandContext(fixture.runtime()),
                                                                fixture.clipA, ParamInfo::Pitch,
                                                                Param::Edited, {curve(20, 6100)});
                const auto snapshot = fixture.runtime().parameters().getParameter(
                    fixture.runtime().documentVersion().documentId, fixture.clipA, ParamInfo::Pitch,
                    Param::Edited);
                const auto changed = fixture.runtime().parameters().replaceParameter(
                    commandContext(fixture.runtime()), fixture.clipA, ParamInfo::Pitch,
                    Param::Edited, {curve(30, 7100)});
                suite.expect(snapshot && changed &&
                                 snapshot.get().curves.first().values ==
                                     QList<int>({6100, 6110, 6120}),
                             QStringLiteral("parameter snapshot must own detached curve data"));
            });
        suite.run(Automation::OperationIds::parameters::get,
                  QStringLiteral("Q4-INVALID-PARAMETER-TYPE"), [&] {
                      Fixture fixture;
                      const auto before = fixture.runtime().documentVersion();
                      const auto result = fixture.runtime().parameters().getParameter(
                          before.documentId, fixture.clipA, ParamInfo::Unknown, Param::Unknown);
                      suite.expect(!result &&
                                       result.getError().code ==
                                           AutomationErrorCode::InvalidArgument &&
                                       result.getError().operationId ==
                                           Automation::OperationIds::parameters::get &&
                                       fixture.runtime().documentVersion() == before,
                                   QStringLiteral("unsupported parameter query must fail safely"));
                  });
        suite.run(
            Automation::OperationIds::parameters::get, QStringLiteral("Q5-NO-SIDE-EFFECTS"), [&] {
                Fixture fixture;
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto beforeHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                const auto result = fixture.runtime().parameters().getParameter(
                    beforeVersion.documentId, fixture.clipA, ParamInfo::Pitch, Param::Edited);
                const auto afterHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                suite.expect(result && fixture.runtime().documentVersion() == beforeVersion &&
                                 fixture.stateFingerprint() == beforeState && beforeHistory &&
                                 afterHistory &&
                                 beforeHistory.get().canUndo == afterHistory.get().canUndo,
                             QStringLiteral("parameter query must have no document side effects"));
            });
    }

    void runTimelineQueryComplements(Suite &suite) {
        suite.run(Automation::OperationIds::timeline::get, QStringLiteral("Q1-MINIMUM-DTO"), [&] {
            AutomationTestSupport::TestRuntime fixture;
            auto &runtime = fixture.runtime();
            const auto result =
                runtime.timeline().getTimeline(runtime.documentVersion().documentId);
            suite.expect(result && result.get().document == runtime.documentVersion() &&
                             result.get().tempos.size() == 1 &&
                             result.get().tempos.first().pos == 0 &&
                             result.get().timeSignatures.size() == 1 &&
                             result.get().timeSignatures.first().barIndex == 0,
                         QStringLiteral("minimum timeline must expose both required anchors"));
        });
        suite.run(Automation::OperationIds::timeline::get, QStringLiteral("Q1-POPULATED-DTO"), [&] {
            Fixture fixture;
            const auto result = fixture.runtime().timeline().getTimeline(
                fixture.runtime().documentVersion().documentId);
            suite.expect(
                result && result.get().tempos.size() == 3 && result.get().tempos.at(1).pos == 960 &&
                    result.get().tempos.at(2).pos == 1920 &&
                    result.get().timeSignatures.size() == 3 &&
                    result.get().timeSignatures.at(1).barIndex == 4 &&
                    result.get().timeSignatures.at(2).barIndex == 8,
                QStringLiteral("timeline DTO must preserve sorted tempo/signature points"));
        });
        suite.run(
            Automation::OperationIds::timeline::get, QStringLiteral("Q2-DETACHED-SNAPSHOT"), [&] {
                Fixture fixture;
                const auto snapshot = fixture.runtime().timeline().getTimeline(
                    fixture.runtime().documentVersion().documentId);
                const auto changed = fixture.runtime().timeline().setTempo(
                    commandContext(fixture.runtime()), 960, 199.0);
                suite.expect(snapshot && changed && snapshot.get().tempos.at(1).value == 135.0,
                             QStringLiteral("timeline snapshot must not alias later tempo edits"));
            });
        suite.run(
            Automation::OperationIds::timeline::get, QStringLiteral("Q4-ANCHOR-BOUNDARIES"), [&] {
                Fixture fixture;
                const auto tempo = fixture.runtime().timeline().setTempo(
                    commandContext(fixture.runtime()), 0, 90.0);
                const auto signature = fixture.runtime().timeline().setTimeSignature(
                    commandContext(fixture.runtime()), 0, 7, 8);
                const auto result = fixture.runtime().timeline().getTimeline(
                    fixture.runtime().documentVersion().documentId);
                suite.expect(
                    tempo && signature && result && result.get().tempos.first().pos == 0 &&
                        result.get().tempos.first().value == 90.0 &&
                        result.get().timeSignatures.first().barIndex == 0 &&
                        result.get().timeSignatures.first().numerator == 7,
                    QStringLiteral("timeline query must retain replacement values at anchors"));
            });
        suite.run(
            Automation::OperationIds::timeline::get, QStringLiteral("Q5-NO-SIDE-EFFECTS"), [&] {
                Fixture fixture;
                const auto beforeVersion = fixture.runtime().documentVersion();
                const auto beforeState = fixture.stateFingerprint();
                const auto beforeHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                const auto result =
                    fixture.runtime().timeline().getTimeline(beforeVersion.documentId);
                const auto afterHistory =
                    fixture.runtime().history().getState(beforeVersion.documentId);
                suite.expect(
                    result && fixture.runtime().documentVersion() == beforeVersion &&
                        fixture.stateFingerprint() == beforeState && beforeHistory &&
                        afterHistory && beforeHistory.get().canUndo == afterHistory.get().canUndo,
                    QStringLiteral("timeline query must not change model, revision, or History"));
            });
    }

    void runHistoryQueryComplements(Suite &suite) {
        suite.run(
            Automation::OperationIds::history::get_state, QStringLiteral("Q1-MINIMUM-DTO"), [&] {
                Fixture fixture;
                const auto result = fixture.runtime().history().getState(
                    fixture.runtime().documentVersion().documentId);
                suite.expect(result &&
                                 result.get().document == fixture.runtime().documentVersion() &&
                                 !result.get().canUndo && !result.get().canRedo &&
                                 result.get().onSavePoint && result.get().undoName.isEmpty() &&
                                 result.get().redoName.isEmpty(),
                             QStringLiteral("empty History must expose complete optional state"));
            });
        suite.run(
            Automation::OperationIds::history::get_state, QStringLiteral("Q1-POPULATED-DTO"), [&] {
                Fixture fixture;
                const auto changed = fixture.runtime().timeline().setTempo(
                    commandContext(fixture.runtime()), 3000, 151.0);
                const auto result = fixture.runtime().history().getState(
                    fixture.runtime().documentVersion().documentId);
                suite.expect(changed && result && result.get().canUndo && !result.get().canRedo &&
                                 !result.get().onSavePoint && !result.get().undoName.isEmpty(),
                             QStringLiteral("committed edit must expose a named undo"));
            });
        suite.run(Automation::OperationIds::history::get_state,
                  QStringLiteral("Q2-DETACHED-SNAPSHOT"), [&] {
                      Fixture fixture;
                      fixture.runtime().timeline().setTempo(commandContext(fixture.runtime()), 3000,
                                                            151.0);
                      const auto snapshot = fixture.runtime().history().getState(
                          fixture.runtime().documentVersion().documentId);
                      const auto undo =
                          fixture.runtime().history().undo(commandContext(fixture.runtime()));
                      suite.expect(
                          snapshot && undo && snapshot.get().canUndo && !snapshot.get().canRedo,
                          QStringLiteral("History DTO must remain detached after stack change"));
                  });
        suite.run(Automation::OperationIds::history::get_state, QStringLiteral("Q4-REDO-OPTIONALS"),
                  [&] {
                      Fixture fixture;
                      fixture.runtime().timeline().setTempo(commandContext(fixture.runtime()), 3000,
                                                            151.0);
                      fixture.runtime().history().undo(commandContext(fixture.runtime()));
                      const auto result = fixture.runtime().history().getState(
                          fixture.runtime().documentVersion().documentId);
                      suite.expect(result && !result.get().canUndo && result.get().canRedo &&
                                       result.get().undoName.isEmpty() &&
                                       !result.get().redoName.isEmpty(),
                                   QStringLiteral("undone stack must expose only the redo name"));
                  });
        suite.run(Automation::OperationIds::history::get_state,
                  QStringLiteral("Q5-NO-SIDE-EFFECTS"), [&] {
                      Fixture fixture;
                      fixture.runtime().timeline().setTempo(commandContext(fixture.runtime()), 3000,
                                                            151.0);
                      const auto beforeVersion = fixture.runtime().documentVersion();
                      const auto beforeState = fixture.stateFingerprint();
                      const auto first =
                          fixture.runtime().history().getState(beforeVersion.documentId);
                      const auto second =
                          fixture.runtime().history().getState(beforeVersion.documentId);
                      suite.expect(
                          first && second && first.get().canUndo == second.get().canUndo &&
                              first.get().undoName == second.get().undoName &&
                              fixture.runtime().documentVersion() == beforeVersion &&
                              fixture.stateFingerprint() == beforeState,
                          QStringLiteral("History query must not consume or mutate stack state"));
                  });
    }

    void runQueryDimensions(Suite &suite) {
        runProjectQueryComplements(suite);
        runNotesQueryComplements(suite);
        runParameterQueryComplements(suite);
        runTimelineQueryComplements(suite);
        runHistoryQueryComplements(suite);
        suite.run(
            Automation::OperationIds::project::get, QStringLiteral("Q3-DOCUMENT-PRIORITY"), [&] {
                Fixture fixture;
                const auto before = fixture.runtime().documentVersion();
                const auto state = fixture.stateFingerprint();
                const auto result =
                    fixture.runtime().project().getProject(Automation::DocumentId::create());
                suite.expect(
                    !result && result.getError().code == AutomationErrorCode::DocumentChanged &&
                        result.getError().operationId == Automation::OperationIds::project::get,
                    QStringLiteral("project query must reject an old DocumentId"));
                suite.expect(fixture.runtime().documentVersion() == before &&
                                 fixture.stateFingerprint() == state,
                             QStringLiteral("rejected project query must be side-effect free"));
            });

        suite.run(
            Automation::OperationIds::notes::list, QStringLiteral("Q3-DOCUMENT-PRIORITY"), [&] {
                Fixture fixture;
                const auto before = fixture.runtime().documentVersion();
                const auto result = fixture.runtime().notes().getNotes(
                    Automation::DocumentId::create(), ClipId(999999));
                suite.expect(
                    !result && result.getError().code == AutomationErrorCode::DocumentChanged &&
                        result.getError().operationId == Automation::OperationIds::notes::list &&
                        fixture.runtime().documentVersion() == before,
                    QStringLiteral("notes query must validate document before clip"));
            });

        suite.run(
            Automation::OperationIds::parameters::get, QStringLiteral("Q3-DOCUMENT-PRIORITY"), [&] {
                Fixture fixture;
                const auto before = fixture.runtime().documentVersion();
                const auto result = fixture.runtime().parameters().getParameter(
                    Automation::DocumentId::create(), ClipId(999999), ParamInfo::Unknown,
                    Param::Unknown);
                suite.expect(!result &&
                                 result.getError().code == AutomationErrorCode::DocumentChanged &&
                                 result.getError().operationId ==
                                     Automation::OperationIds::parameters::get &&
                                 fixture.runtime().documentVersion() == before,
                             QStringLiteral("parameter query must validate document first"));
            });

        suite.run(
            Automation::OperationIds::timeline::get, QStringLiteral("Q3-DOCUMENT-PRIORITY"), [&] {
                Fixture fixture;
                const auto before = fixture.runtime().documentVersion();
                const auto result =
                    fixture.runtime().timeline().getTimeline(Automation::DocumentId::create());
                suite.expect(
                    !result && result.getError().code == AutomationErrorCode::DocumentChanged &&
                        result.getError().operationId == Automation::OperationIds::timeline::get &&
                        fixture.runtime().documentVersion() == before,
                    QStringLiteral("timeline query must reject an old generation"));
            });

        suite.run(Automation::OperationIds::history::get_state,
                  QStringLiteral("Q3-DOCUMENT-PRIORITY"), [&] {
                      Fixture fixture;
                      const auto before = fixture.runtime().documentVersion();
                      const auto result =
                          fixture.runtime().history().getState(Automation::DocumentId::create());
                      suite.expect(!result &&
                                       result.getError().code ==
                                           AutomationErrorCode::DocumentChanged &&
                                       result.getError().operationId ==
                                           Automation::OperationIds::history::get_state &&
                                       fixture.runtime().documentVersion() == before,
                                   QStringLiteral("History query must reject an old generation"));
                  });
    }

    QList<OperationId> coveredOperations(const QList<CommandSpec> &specs) {
        QList<OperationId> operations = {
            Automation::OperationIds::project::get,       Automation::OperationIds::notes::list,
            Automation::OperationIds::parameters::get,    Automation::OperationIds::timeline::get,
            Automation::OperationIds::history::get_state,
        };
        for (const auto &spec : specs)
            operations.append(spec.operationId);
        return operations;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Suite suite;
    auto specs = highValueSpecs();
    specs.append(remainingSpecs());
    runHighValueCommandDimensions(suite, specs);
    runQueryDimensions(suite);
    suite.requireOperations(coveredOperations(specs));
    return suite.result();
}
