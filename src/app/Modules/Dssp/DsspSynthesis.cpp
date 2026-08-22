#include "DsspSynthesis.h"

#include "DsspMetadata.h"

#include "Model/AppOptions/AppOptions.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/VersionUtils.h>

#include <diffsinger/Bank/SingerSnapshot.h>
#include <diffsinger/Infer/StageKind.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Common/1/CommonApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Vocoder/2/VocoderApiL2.h>
#include <diffsinger/Session/ModelSetHandle.h>
#include <diffsinger/Session/VoicebankSession.h>

#include <sndfile.hh>

#include <QLoggingCategory>
#include <QVersionNumber>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <semaphore>

Q_LOGGING_CATEGORY(logDsspSynthesis, "dssp.synthesis")

namespace DsspSynthesis {

    namespace Co = srt::svs::Api::Common::L1;
    namespace Dur = srt::svs::Api::Duration::L1;
    namespace Pit = srt::svs::Api::Pitch::L1;
    namespace Var = srt::svs::Api::Variance::L1;
    namespace Ac = srt::svs::Api::Acoustic::L1;
    namespace Vo1 = srt::svs::Api::Vocoder::L1;
    namespace Vo2 = srt::svs::Api::Vocoder::L2;

    namespace {

        // === Concurrency scope ===
        //
        // Document synthesis runs through InferController's per-stage task
        // queues sharing per-singer ModelSets. HTTP synthesis uses per-request
        // ModelSetHandle instances (created by VoicebankSession::ensureModelSet),
        // so inference objects are fully isolated and thread-safe. Both paths,
        // however, consume the same GPU/ONNX resources, so HTTP syntheses are
        // serialized by a global semaphore to avoid unbounded session/VRAM
        // growth when document and HTTP synthesis run concurrently.
        constexpr int kMaxConcurrentSyntheses = 1;
        std::counting_semaphore<kMaxConcurrentSyntheses> g_synthesisSlots{kMaxConcurrentSyntheses};

        /// RAII guard releasing the global synthesis slot on scope exit.
        struct SynthesisSlotGuard {
            SynthesisSlotGuard() { g_synthesisSlots.acquire(); }
            ~SynthesisSlotGuard() { g_synthesisSlots.release(); }
        };

        // === Parameter specs (API value domain == document value domain) ===
        struct ParameterSpec {
            QString id;
            srt::svs::ParamTag tag;
            double min;
            double max;
            std::function<double(double)> transform;
            std::function<double(double)> inverseTransform;
        };

        const std::vector<ParameterSpec> &parameterSpecs() {
            static const std::vector<ParameterSpec> specs = {
                {QStringLiteral("pitch"), Co::Tags::Pitch, 0, 12800,
                 [](double v) { return v / 100.0; }, [](double v) { return v * 100.0; }},
                {QStringLiteral("expressiveness"), Co::Tags::Expr, 0, 1000,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("energy"), Co::Tags::Energy, -96000, 0,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("breathiness"), Co::Tags::Breathiness, -96000, 0,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("voicing"), Co::Tags::Voicing, -96000, 0,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("tension"), Co::Tags::Tension, -10000, 10000,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("mouth_opening"), Co::Tags::MouthOpening, 0, 1000,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("gender"), Co::Tags::Gender, -1000, 1000,
                 [](double v) { return v / 1000.0; }, [](double v) { return v * 1000.0; }},
                {QStringLiteral("velocity"), Co::Tags::Velocity, -1000, 1000,
                 [](double v) { return std::exp2(v / 1000.0); },
                 [](double v) { return std::log2(v) * 1000.0; }},
                {QStringLiteral("tone_shift"), Co::Tags::ToneShift, -1200, 1200,
                 [](double v) { return v / 100.0; }, [](double v) { return v * 100.0; }},
            };
            return specs;
        }

        const ParameterSpec *findSpec(const QString &id) {
            for (const auto &spec : parameterSpecs()) {
                if (spec.id == id)
                    return &spec;
            }
            return nullptr;
        }

        const ParameterSpec *findSpec(const srt::svs::ParamTag &tag) {
            for (const auto &spec : parameterSpecs()) {
                if (spec.tag.name() == tag.name())
                    return &spec;
            }
            return nullptr;
        }

        const QStringList varianceIds{
            QStringLiteral("energy"),    QStringLiteral("breathiness"),
            QStringLiteral("voicing"),   QStringLiteral("tension"),
            QStringLiteral("mouth_opening"),
        };

        const QStringList audioParameterIds{
            QStringLiteral("pitch"),       QStringLiteral("breathiness"),
            QStringLiteral("tension"),     QStringLiteral("voicing"),
            QStringLiteral("energy"),      QStringLiteral("mouth_opening"),
            QStringLiteral("gender"),      QStringLiteral("velocity"),
            QStringLiteral("tone_shift"),
        };

        // === API note/word model ===
        struct ApiPhoneme {
            QString token;
            double start = 0;
            bool onset = false;
            QString language;
        };

        struct ApiNote {
            double gap = 0;
            double duration = 0;
            int cent = 0;
            QString pronunciation;
            QString language;
            QList<ApiPhoneme> phonemes;
        };

        struct PlacedNote {
            ApiNote note;
            double start = 0;
            double end = 0;
        };

        struct BuiltWord {
            double start = 0;
            Co::InputWordInfo word;
        };

        // === Resolved synthesis context ===
        struct ResolvedSinger {
            QString apiId;
            DsspMetadata::SingerReference ref;
            QString speaker;
            std::string mappedSpeaker;
        };

        struct ResolvedContext {
            std::vector<ResolvedSinger> singers;
            std::vector<std::vector<double>> mix; // N-1 values per row
            double mixSampleRate = 0;
            double pieceDuration = 0;
            int64_t steps = 0;
            float depth = 0;
        };

        std::string toUtf8(const QString &value) {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }

        bool isRest(const QString &pronunciation) {
            return pronunciation == QStringLiteral("SP") || pronunciation == QStringLiteral("AP");
        }

        bool isSlur(const QString &pronunciation) {
            return pronunciation == QStringLiteral("-");
        }

        std::optional<DsspApi::Problem> validateSingerSpeaker(
            const ds::bank::SingerSnapshot *snapshot, const QString &speaker) {
            if (snapshot->speakerIds.empty()) {
                if (!speaker.isEmpty()) {
                    return DsspApi::singerConfigInvalid(
                        QStringLiteral("Singer does not define speakers"));
                }
                return std::nullopt;
            }
            for (const auto &id : snapshot->speakerIds) {
                if (QString::fromStdString(id) == speaker)
                    return std::nullopt;
            }
            return DsspApi::singerConfigInvalid(
                QStringLiteral("Speaker \"%1\" does not exist").arg(speaker));
        }

        std::optional<DsspApi::Problem> resolveContext(const QJsonObject &body,
                                                       ResolvedContext &out) {
            QJsonObject context;
            if (!DsspApi::readObject(body, QStringLiteral("context"), context)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context\""));
            }
            QString arch;
            if (!DsspApi::readString(context, QStringLiteral("arch"), arch)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context.arch\""));
            }
            if (arch != QLatin1String("diffsinger"))
                return DsspApi::unknownArch(arch);

            out.steps = appOptions->inference()->samplingSteps;
            out.depth = static_cast<float>(appOptions->inference()->depth);
            if (context.contains(QStringLiteral("arch_extra"))) {
                const auto archExtra = context.value(QStringLiteral("arch_extra")).toObject();
                if (archExtra.contains(QStringLiteral("steps"))) {
                    const auto stepsValue = archExtra.value(QStringLiteral("steps")).toDouble();
                    if (stepsValue < 0 || std::floor(stepsValue) != stepsValue) {
                        return DsspApi::singerConfigInvalid(
                            QStringLiteral("Invalid arch_extra.steps"));
                    }
                    out.steps = static_cast<int64_t>(stepsValue);
                }
                if (archExtra.contains(QStringLiteral("depth"))) {
                    const auto depthValue = archExtra.value(QStringLiteral("depth")).toDouble();
                    if (!std::isfinite(depthValue)) {
                        return DsspApi::singerConfigInvalid(
                            QStringLiteral("Invalid arch_extra.depth"));
                    }
                    out.depth = static_cast<float>(depthValue);
                }
            }

            QJsonArray singers;
            if (!DsspApi::readArray(context, QStringLiteral("singers"), singers) ||
                singers.isEmpty()) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context.singers\""));
            }

            QString mixGroup;
            for (const auto &singerValue : singers) {
                if (!singerValue.isObject()) {
                    return DsspApi::validationError(
                        QStringLiteral("Invalid singer in \"context.singers\""));
                }
                const auto singerObj = singerValue.toObject();
                ResolvedSinger singer;
                if (!DsspApi::readString(singerObj, QStringLiteral("id"), singer.apiId) ||
                    !DsspMetadata::parseSingerId(singer.apiId, singer.ref)) {
                    return DsspApi::singerNotExist(singer.apiId);
                }
                QJsonObject extra;
                if (!DsspApi::readObject(singerObj, QStringLiteral("extra"), extra) ||
                    !DsspApi::readString(extra, QStringLiteral("speaker"), singer.speaker)) {
                    return DsspApi::singerConfigInvalid(
                        QStringLiteral("Singer extra must identify a speaker"));
                }
                const auto *snapshot = DsspMetadata::findSinger(singer.ref);
                if (!snapshot)
                    return DsspApi::singerNotExist(singer.apiId);
                if (auto problem = validateSingerSpeaker(snapshot, singer.speaker))
                    return problem;

                const auto group = QStringLiteral("%1@%2").arg(singer.ref.packageId,
                                                               singer.ref.version);
                if (mixGroup.isEmpty())
                    mixGroup = group;
                else if (mixGroup != group) {
                    return DsspApi::singersUnmixable(
                        out.singers.front().apiId, singer.apiId,
                        QStringLiteral("Singers use different inference resources"));
                }
                out.singers.push_back(std::move(singer));
            }

            return std::nullopt;
        }

        // === Mix handling ===
        // Per-frame singer mix data: each row has singers.length - 1 values with
        // row sum <= 1; the last singer receives the remainder.
        std::optional<DsspApi::Problem> validateMix(const QJsonArray &mixValue, double sampleRate,
                                                    size_t singerCount,
                                                    std::vector<std::vector<double>> &out) {
            if (!(sampleRate > 0) || !std::isfinite(sampleRate)) {
                return DsspApi::validationError(
                    QStringLiteral("\"mix_sample_rate\" must be positive and finite"));
            }
            if (singerCount > 1 && mixValue.isEmpty()) {
                return DsspApi::validationError(
                    QStringLiteral("\"mix\" cannot be empty for multiple singers"));
            }
            const auto expectedLength = singerCount - 1;
            out.reserve(static_cast<size_t>(mixValue.size()));
            for (const auto &rowValue : mixValue) {
                if (!rowValue.isArray()) {
                    return DsspApi::validationError(QStringLiteral("Invalid \"mix\" row"));
                }
                const auto rowArray = rowValue.toArray();
                if (static_cast<size_t>(rowArray.size()) != expectedLength) {
                    return DsspApi::validationError(
                        QStringLiteral("\"mix\" row has %1 values, expected %2")
                            .arg(rowArray.size())
                            .arg(static_cast<qint64>(expectedLength)));
                }
                std::vector<double> row;
                row.reserve(expectedLength);
                double sum = 0;
                for (const auto &value : rowArray) {
                    const auto number = value.toDouble();
                    if (!std::isfinite(number) || number < 0 || number > 1)
                        return DsspApi::validationError(QStringLiteral("Invalid \"mix\" value"));
                    sum += number;
                    row.push_back(number);
                }
                if (sum > 1 + 1e-9)
                    return DsspApi::validationError(
                        QStringLiteral("\"mix\" row sum must not exceed 1"));
                out.push_back(std::move(row));
            }
            return std::nullopt;
        }

        std::vector<double> completeProportions(const std::vector<double> &row, size_t count) {
            std::vector<double> result(count, 0.0);
            double sum = 0;
            for (size_t i = 0; i < row.size(); ++i) {
                result[i] = row[i];
                sum += row[i];
            }
            result[count - 1] = 1.0 - sum;
            return result;
        }

        std::vector<double> sampleMix(const std::vector<std::vector<double>> &mix,
                                      double sampleRate, size_t singerCount, double time) {
            if (singerCount == 1)
                return {1.0};
            if (time <= 0 || mix.size() == 1)
                return completeProportions(mix.front(), singerCount);
            const auto position = time * sampleRate;
            const auto lastIndex = static_cast<double>(mix.size() - 1);
            if (position >= lastIndex)
                return completeProportions(mix.back(), singerCount);
            const auto leftIndex = static_cast<size_t>(std::floor(position));
            const auto rightIndex = leftIndex + 1;
            const auto weight = position - std::floor(position);
            std::vector<double> sample(singerCount - 1, 0.0);
            for (size_t i = 0; i < sample.size(); ++i)
                sample[i] = mix[leftIndex][i] * (1 - weight) + mix[rightIndex][i] * weight;
            return completeProportions(sample, singerCount);
        }

        // === Word building (mirrors the DSSP reference BuildWords) ===
        struct BuildWordsResult {
            std::vector<BuiltWord> words;
            // Word-level mapping: for each built word, the note/phoneme targets.
            struct Target {
                int noteIndex = -1;
                int phonemeIndex = -1;
                double noteStart = 0;
                bool valid = false;
            };
            std::vector<std::vector<Target>> mapping;
        };

        std::optional<DsspApi::Problem> buildWords(const QList<ApiNote> &apiNotes,
                                                   const std::vector<std::string> &speakerNames,
                                                   const std::vector<std::vector<double>> &mix,
                                                   double mixSampleRate, BuildWordsResult &out) {
            // A phoneme together with its origin (note/phoneme indices) so the
            // header/body split can later map inference outputs back to the
            // original note phoneme slots (mirrors the DSSP reference).
            struct PhonemeSource {
                ApiPhoneme phoneme;
                int noteIndex = 0;
                int phonemeIndex = -1;
                double noteStart = 0;
                bool valid = false;
            };

            std::vector<PlacedNote> placed;
            double position = 0;
            for (int i = 0; i < apiNotes.size(); ++i) {
                const auto &note = apiNotes.at(i);
                if (note.gap < 0)
                    return DsspApi::validationError(
                        QStringLiteral("Note %1 gap cannot be negative").arg(i));
                if (note.duration < 0)
                    return DsspApi::validationError(
                        QStringLiteral("Note %1 duration cannot be negative").arg(i));
                position += note.gap;
                placed.push_back({note, position, position + note.duration});
                position += note.duration;
            }
            if (placed.empty())
                return std::nullopt;
            if (isSlur(placed.front().note.pronunciation))
                return DsspApi::validationError(QStringLiteral("First note cannot be a slur"));

            const auto sourcesOf = [](const PlacedNote &note, int noteIndex) {
                QList<PhonemeSource> sources;
                if (!note.note.phonemes.isEmpty() || !isRest(note.note.pronunciation)) {
                    for (int i = 0; i < note.note.phonemes.size(); ++i)
                        sources.append({note.note.phonemes.at(i), noteIndex, i, note.start, true});
                } else {
                    // Rest note without phonemes: synthetic onset placeholder that
                    // does not map back to any real note phoneme.
                    sources.append(
                        {{note.note.pronunciation, 0, true, note.note.language}, noteIndex, -1,
                         note.start, false});
                }
                return sources;
            };
            const auto splitHeaderAndBody = [](const QList<PhonemeSource> &phonemes)
                -> std::pair<QList<PhonemeSource>, QList<PhonemeSource>> {
                for (int i = 0; i < phonemes.size(); ++i) {
                    if (phonemes.at(i).phoneme.onset)
                        return {phonemes.mid(0, i), phonemes.mid(i)};
                }
                return {phonemes, {}};
            };
            const auto appendPhone = [](BuiltWord &word, const PhonemeSource &source,
                                        double start) {
                word.word.phones.push_back(
                    Co::InputPhonemeInfo{toUtf8(source.phoneme.token),
                                         toUtf8(source.phoneme.language), 0, start, {}});
            };
            // Push one mapping target per appended phoneme: valid sources map
            // back to their note phoneme slot, placeholders keep an empty slot
            // so mapping counts always match word phoneme counts.
            const auto appendTarget = [](std::vector<BuildWordsResult::Target> &targets,
                                         const PhonemeSource &source) {
                if (source.valid)
                    targets.push_back(
                        {source.noteIndex, source.phonemeIndex, source.noteStart, true});
                else
                    targets.push_back({});
            };

            std::vector<BuiltWord> words;
            std::vector<std::vector<BuildWordsResult::Target>> mapping;

            // Leading gap word (SP header).
            if (placed.front().start > 0) {
                const auto [header, bodyIgnored] = splitHeaderAndBody(sourcesOf(placed.front(), 0));
                BuiltWord word;
                word.start = 0;
                word.word.notes.push_back(Co::InputNoteInfo{/* key */ 0, /* cents */ 0,
                                                            placed.front().start, Co::GT_None,
                                                            true});
                word.word.phones.push_back(Co::InputPhonemeInfo{
                    toUtf8(QStringLiteral("SP")), toUtf8(placed.front().note.language), 0, 0, {}});
                std::vector<BuildWordsResult::Target> targets;
                targets.push_back({});
                for (const auto &source : header) {
                    appendPhone(word, source, placed.front().start + source.phoneme.start);
                    appendTarget(targets, source);
                }
                words.push_back(std::move(word));
                mapping.push_back(std::move(targets));
            }

            const auto hasEmptyNonRestPhonemes = [](const PlacedNote &note) {
                return note.note.phonemes.isEmpty() && !isRest(note.note.pronunciation);
            };

            for (int noteIndex = 0; noteIndex < static_cast<int>(placed.size()); ++noteIndex) {
                const auto &note = placed.at(static_cast<size_t>(noteIndex));
                const auto wordStart = note.start;
                auto wordEnd = note.end;
                BuiltWord word;
                word.start = wordStart;
                word.word.notes.push_back(Co::InputNoteInfo{
                    note.note.cent / 100, note.note.cent % 100, note.note.duration,
                    Co::GT_None, isRest(note.note.pronunciation)});
                auto keepEmpty = hasEmptyNonRestPhonemes(note);
                const auto [headerIgnored, body] = splitHeaderAndBody(sourcesOf(note, noteIndex));
                std::vector<BuildWordsResult::Target> targets;
                for (const auto &source : body) {
                    appendPhone(word, source, source.phoneme.start);
                    appendTarget(targets, source);
                }

                while (noteIndex + 1 < static_cast<int>(placed.size()) &&
                       isSlur(placed.at(static_cast<size_t>(noteIndex + 1)).note.pronunciation)) {
                    const auto &next = placed.at(static_cast<size_t>(noteIndex + 1));
                    word.word.notes.push_back(Co::InputNoteInfo{
                        next.note.cent / 100, next.note.cent % 100, next.note.duration,
                        Co::GT_None, isRest(next.note.pronunciation)});
                    keepEmpty = keepEmpty || hasEmptyNonRestPhonemes(next);
                    wordEnd = next.end;
                    ++noteIndex;
                }

                const auto keepWord = [](const BuiltWord &word, bool keepEmpty) {
                    return !word.word.phones.empty() || keepEmpty;
                };

                if (noteIndex + 1 >= static_cast<int>(placed.size())) {
                    if (keepWord(word, keepEmpty)) {
                        words.push_back(std::move(word));
                        mapping.push_back(std::move(targets));
                    }
                    continue;
                }

                const auto &next = placed.at(static_cast<size_t>(noteIndex + 1));
                auto gapLength = next.start - wordEnd;
                if (gapLength < 0)
                    gapLength = 0;
                const auto [header, bodyIgnored0] =
                    splitHeaderAndBody(sourcesOf(next, noteIndex + 1));
                const auto attachBase = gapLength > 0 ? gapLength : wordEnd - wordStart;

                if (gapLength == 0) {
                    for (const auto &source : header) {
                        appendPhone(word, source, attachBase + source.phoneme.start);
                        appendTarget(targets, source);
                    }
                }
                if (keepWord(word, keepEmpty)) {
                    words.push_back(std::move(word));
                    mapping.push_back(std::move(targets));
                }
                if (gapLength > 0) {
                    BuiltWord gapWord;
                    gapWord.start = wordEnd;
                    gapWord.word.notes.push_back(Co::InputNoteInfo{
                        placed.at(static_cast<size_t>(noteIndex)).note.cent / 100,
                        placed.at(static_cast<size_t>(noteIndex)).note.cent % 100, gapLength,
                        Co::GT_None, true});
                    gapWord.word.phones.push_back(Co::InputPhonemeInfo{
                        toUtf8(QStringLiteral("SP")), toUtf8(next.note.language), 0, 0, {}});
                    std::vector<BuildWordsResult::Target> gapTargets;
                    gapTargets.push_back({});
                    for (const auto &source : header) {
                        appendPhone(gapWord, source, gapLength + source.phoneme.start);
                        appendTarget(gapTargets, source);
                    }
                    words.push_back(std::move(gapWord));
                    mapping.push_back(std::move(gapTargets));
                }
            }

            // Attach per-phoneme static speaker proportions sampled from the mix.
            for (auto &built : words) {
                for (auto &phone : built.word.phones) {
                    const auto proportions =
                        sampleMix(mix, mixSampleRate, speakerNames.size(), built.start + phone.start);
                    for (size_t i = 0; i < speakerNames.size(); ++i)
                        phone.speakers.push_back({speakerNames[i], proportions[i]});
                }
            }

            out.words = std::move(words);
            out.mapping = std::move(mapping);
            return std::nullopt;
        }

        // === Stage access helpers ===
        struct StageInputs {
            std::shared_ptr<ds::session::ModelSetHandle> handle;
            std::map<std::string, std::string> durationSpeakerMapping;
            std::map<std::string, std::string> pitchSpeakerMapping;
            std::map<std::string, std::string> varianceSpeakerMapping;
            std::map<std::string, std::string> acousticSpeakerMapping;
        };

        std::optional<DsspApi::Problem> acquireStages(const ResolvedContext &context,
                                                      StageInputs &out) {
            auto &session = SynthrtEngine::instance().session();
            const auto &ref = context.singers.front().ref;
            ds::bank::SingerRef singerKey{ref.packageId.toStdString(), ref.singerId.toStdString(),
                                          ref.version.toStdString()};
            auto handleExp = session.ensureModelSet(singerKey);
            if (!handleExp) {
                return DsspApi::internalError(
                    QStringLiteral("Failed to load singer models: %1")
                        .arg(QString::fromUtf8(handleExp.error().message())));
            }
            out.handle = handleExp.take();
            const auto &stages = out.handle->stages();

            if (const auto *stage = stages.find(ds::infer::StageKind::Duration)) {
                if (const auto options = stage->options.as<Dur::DurationImportOptions>())
                    out.durationSpeakerMapping = options->speakerMapping;
            }
            if (const auto *stage = stages.find(ds::infer::StageKind::Pitch)) {
                if (const auto options = stage->options.as<Pit::PitchImportOptions>())
                    out.pitchSpeakerMapping = options->speakerMapping;
            }
            if (const auto *stage = stages.find(ds::infer::StageKind::Variance)) {
                if (const auto options = stage->options.as<Var::VarianceImportOptions>())
                    out.varianceSpeakerMapping = options->speakerMapping;
            }
            if (const auto *stage = stages.find(ds::infer::StageKind::Acoustic)) {
                if (const auto options = stage->options.as<Ac::AcousticImportOptions>())
                    out.acousticSpeakerMapping = options->speakerMapping;
            }
            return std::nullopt;
        }

        std::string mapSpeaker(const std::map<std::string, std::string> &mapping,
                               const QString &speaker, QString &error) {
            const auto name = toUtf8(speaker);
            if (mapping.empty())
                return name;
            const auto it = mapping.find(name);
            if (it != mapping.end())
                return it->second;
            error = QStringLiteral("Speaker mapping not found for speaker %1").arg(speaker);
            return {};
        }

        /// Fill per-stage speaker names for all singers via the given mapping.
        std::optional<DsspApi::Problem> fillMappedSpeakers(
            std::vector<ResolvedSinger> &singers,
            const std::map<std::string, std::string> &mapping) {
            QString error;
            for (auto &singer : singers) {
                singer.mappedSpeaker = mapSpeaker(mapping, singer.speaker, error);
                if (!error.isEmpty())
                    return DsspApi::singerConfigInvalid(error);
            }
            return std::nullopt;
        }

        std::vector<std::string> mappedSpeakerNames(const std::vector<ResolvedSinger> &singers) {
            std::vector<std::string> names;
            for (const auto &singer : singers)
                names.push_back(singer.mappedSpeaker);
            return names;
        }

        /// Frame-level speaker curves for Pitch/Variance/Acoustic.
        std::vector<Co::InputSpeakerInfo>
            buildDynamicSpeakers(const std::vector<ResolvedSinger> &singers,
                                 const std::vector<std::vector<double>> &mix,
                                 double mixSampleRate) {
            std::vector<Co::InputSpeakerInfo> result;
            const auto frameCount = mix.size();
            for (size_t s = 0; s < singers.size(); ++s) {
                Co::InputSpeakerInfo info;
                info.name = singers[s].mappedSpeaker;
                info.interval = 1.0 / mixSampleRate;
                info.proportions.reserve(frameCount);
                for (const auto &row : mix)
                    info.proportions.push_back(completeProportions(row, singers.size())[s]);
                if (frameCount == 0)
                    info.proportions = {1.0};
                result.push_back(std::move(info));
            }
            return result;
        }

        // === Input parsing ===
        struct ParsedNotesResult {
            QList<ApiNote> notes;
            bool hasPhonemeStarts = false;
        };

        std::optional<DsspApi::Problem> parseNote(const QJsonObject &noteObj, bool withPhonemeStarts,
                                                  ApiNote &out) {
            QJsonObject position;
            if (!DsspApi::readObject(noteObj, QStringLiteral("position"), position) ||
                !DsspApi::readDouble(position, QStringLiteral("gap"), out.gap) ||
                !DsspApi::readDouble(position, QStringLiteral("duration"), out.duration)) {
                return DsspApi::validationError(
                    QStringLiteral("Invalid note position in \"input.notes\""));
            }
            if (!DsspApi::readInt(noteObj, QStringLiteral("cent"), out.cent))
                return DsspApi::validationError(QStringLiteral("Invalid note cent"));
            if (out.cent < 0 || out.cent > 12800)
                return DsspApi::validationError(
                    QStringLiteral("Note cent out of range: %1").arg(out.cent));
            if (!DsspApi::readString(noteObj, QStringLiteral("pronunciation"),
                                     out.pronunciation) ||
                !DsspApi::readString(noteObj, QStringLiteral("language"), out.language)) {
                return DsspApi::validationError(QStringLiteral("Invalid note"));
            }
            QJsonArray phonemes;
            if (DsspApi::readArray(noteObj, QStringLiteral("phonemes"), phonemes)) {
                for (const auto &phonemeValue : phonemes) {
                    if (!phonemeValue.isObject())
                        return DsspApi::validationError(
                            QStringLiteral("Invalid phoneme in \"input.notes\""));
                    const auto phonemeObj = phonemeValue.toObject();
                    ApiPhoneme phoneme;
                    if (!DsspApi::readString(phonemeObj, QStringLiteral("token"), phoneme.token) ||
                        !DsspApi::readBool(phonemeObj, QStringLiteral("onset"), phoneme.onset) ||
                        !DsspApi::readString(phonemeObj, QStringLiteral("language"),
                                             phoneme.language)) {
                        return DsspApi::validationError(
                            QStringLiteral("Invalid phoneme in \"input.notes\""));
                    }
                    if (withPhonemeStarts) {
                        if (!DsspApi::readDouble(phonemeObj, QStringLiteral("start"), phoneme.start))
                            return DsspApi::validationError(
                                QStringLiteral("Invalid phoneme start in \"input.notes\""));
                    }
                    out.phonemes.append(phoneme);
                }
            }
            return std::nullopt;
        }

        std::optional<DsspApi::Problem> parseNotesInput(const QJsonObject &body,
                                                        bool withPhonemeStarts, ResolvedContext &ctx,
                                                        ParsedNotesResult &out) {
            QJsonObject input;
            if (!DsspApi::readObject(body, QStringLiteral("input"), input)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"input\""));
            }
            if (!DsspApi::readDouble(input, QStringLiteral("piece_duration"), ctx.pieceDuration) ||
                ctx.pieceDuration < 0) {
                return DsspApi::validationError(
                    QStringLiteral("Invalid \"input.piece_duration\""));
            }
            QJsonArray notes;
            if (!DsspApi::readArray(input, QStringLiteral("notes"), notes)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"input.notes\""));
            }
            for (const auto &noteValue : notes) {
                if (!noteValue.isObject())
                    return DsspApi::validationError(QStringLiteral("Invalid note"));
                ApiNote note;
                if (auto problem = parseNote(noteValue.toObject(), withPhonemeStarts, note))
                    return problem;
                out.notes.append(note);
            }
            QJsonArray mix;
            if (!DsspApi::readArray(input, QStringLiteral("mix"), mix) ||
                !DsspApi::readDouble(input, QStringLiteral("mix_sample_rate"),
                                     ctx.mixSampleRate)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"input.mix\"/\"input.mix_sample_rate\""));
            }
            return validateMix(mix, ctx.mixSampleRate, ctx.singers.size(), ctx.mix);
        }

        // === Generic per-stage runner ===
        template <typename ResultType>
        std::optional<DsspApi::Problem> runStage(StageInputs &stages, ds::infer::StageKind kind,
                                                 const srt::core::NO<srt::core::TaskStartInput> &input,
                                                 const char *stageName,
                                                 srt::core::NO<ResultType> &outResult) {
            auto loadExp = stages.handle->load(kind);
            if (!loadExp) {
                return DsspApi::internalError(
                    QStringLiteral("Failed to load %1 model: %2")
                        .arg(QString::fromUtf8(stageName))
                        .arg(QString::fromUtf8(loadExp.error().message())));
            }
            auto inference = loadExp.take();
            auto startExp = inference->start(input);
            if (!startExp) {
                return DsspApi::internalError(
                    QStringLiteral("Failed to start %1 inference: %2")
                        .arg(QString::fromUtf8(stageName))
                        .arg(QString::fromUtf8(startExp.error().message())));
            }
            auto result = startExp.take().as<ResultType>();
            if (!result) {
                return DsspApi::internalError(
                    QStringLiteral("%1 result type mismatch or null result")
                        .arg(QString::fromUtf8(stageName)));
            }
            if (!result->error.ok()) {
                return DsspApi::internalError(
                    QStringLiteral("%1 inference failed: %2")
                        .arg(QString::fromUtf8(stageName))
                        .arg(QString::fromUtf8(result->error.message())));
            }
            outResult = std::move(result);
            return std::nullopt;
        }

        // === Parameter building ===
        std::optional<DsspApi::Problem> buildParameter(const QJsonObject &paramObj,
                                                       const QString &id, bool retake,
                                                       std::vector<double> defaultValue,
                                                       Co::InputParameterInfo &out) {
            const auto *spec = findSpec(id);
            if (!spec)
                return DsspApi::invalidParameter(id, QStringLiteral("unknown"),
                                                QStringLiteral("Unknown parameter \"%1\"").arg(id));
            double sampleRate = 0;
            if (!DsspApi::readDouble(paramObj, QStringLiteral("sample_rate"), sampleRate) ||
                !(sampleRate > 0) || !std::isfinite(sampleRate)) {
                return DsspApi::invalidParameter(
                    id, QStringLiteral("invalid_value"),
                    QStringLiteral("Parameter \"%1\" sample rate must be positive and finite")
                        .arg(id));
            }

            std::vector<double> values;
            QJsonArray valuesArray;
            if (DsspApi::readArray(paramObj, QStringLiteral("values"), valuesArray)) {
                values.reserve(static_cast<size_t>(valuesArray.size()));
                for (const auto &value : valuesArray) {
                    const auto number = value.toDouble();
                    if (!std::isfinite(number) || number < spec->min || number > spec->max) {
                        return DsspApi::invalidParameter(
                            id, QStringLiteral("invalid_value"),
                            QStringLiteral("Parameter \"%1\" value out of range [%2, %3]")
                                .arg(id)
                                .arg(spec->min)
                                .arg(spec->max));
                    }
                    values.push_back(number);
                }
            }
            if (values.empty())
                values = std::move(defaultValue);

            out.tag = spec->tag;
            out.interval = 1.0 / sampleRate;
            out.values.reserve(values.size());
            for (const auto value : values)
                out.values.push_back(spec->transform(value));

            if (retake) {
                QJsonObject retakeObj;
                if (!DsspApi::readObject(paramObj, QStringLiteral("retake"), retakeObj)) {
                    return DsspApi::invalidParameter(
                        id, QStringLiteral("retake_required"),
                        QStringLiteral("Missing \"%1\" retake").arg(id));
                }
                int position = 0;
                int length = 0;
                if (!DsspApi::readInt(retakeObj, QStringLiteral("position"), position) ||
                    !DsspApi::readInt(retakeObj, QStringLiteral("length"), length) ||
                    position < 0 || length < 0) {
                    return DsspApi::invalidParameter(id, QStringLiteral("invalid_value"),
                                                     QStringLiteral("Invalid \"%1\" retake").arg(id));
                }
                out.retake = Co::InputParameterInfo::RetakeRange{
                    static_cast<double>(position) / sampleRate,
                    static_cast<double>(position + length) / sampleRate};
            }
            return std::nullopt;
        }

        // === WAV encoding (libsndfile memory IO) ===
        struct MemoryBuffer {
            std::vector<char> data;
            size_t pos = 0;
        };

        sf_count_t vioGetFilelen(void *userData) {
            return static_cast<sf_count_t>(static_cast<MemoryBuffer *>(userData)->data.size());
        }

        sf_count_t vioSeek(sf_count_t offset, int whence, void *userData) {
            auto *buffer = static_cast<MemoryBuffer *>(userData);
            sf_count_t base = 0;
            switch (whence) {
                case SEEK_SET:
                    base = 0;
                    break;
                case SEEK_CUR:
                    base = static_cast<sf_count_t>(buffer->pos);
                    break;
                case SEEK_END:
                    base = static_cast<sf_count_t>(buffer->data.size());
                    break;
                default:
                    return -1;
            }
            const auto target = base + offset;
            if (target < 0)
                return -1;
            buffer->pos = static_cast<size_t>(target);
            return target;
        }

        sf_count_t vioRead(void *ptr, sf_count_t count, void *userData) {
            auto *buffer = static_cast<MemoryBuffer *>(userData);
            const auto available =
                static_cast<sf_count_t>(buffer->data.size() - std::min(buffer->pos,
                                                                       buffer->data.size()));
            const auto toRead = std::min(count, available);
            if (toRead > 0) {
                std::memcpy(ptr, buffer->data.data() + buffer->pos, static_cast<size_t>(toRead));
                buffer->pos += static_cast<size_t>(toRead);
            }
            return toRead;
        }

        sf_count_t vioWrite(const void *ptr, sf_count_t count, void *userData) {
            auto *buffer = static_cast<MemoryBuffer *>(userData);
            const auto bytes = static_cast<const char *>(ptr);
            const auto required = buffer->pos + static_cast<size_t>(count);
            if (required > buffer->data.size())
                buffer->data.resize(required);
            std::memcpy(buffer->data.data() + buffer->pos, bytes, static_cast<size_t>(count));
            buffer->pos += static_cast<size_t>(count);
            return count;
        }

        sf_count_t vioTell(void *userData) {
            return static_cast<sf_count_t>(static_cast<MemoryBuffer *>(userData)->pos);
        }

        std::optional<DsspApi::Problem> encodeWav(const std::vector<float> &audio, int sampleRate,
                                                  int channels, QByteArray &out) {
            static SF_VIRTUAL_IO vio{
                &vioGetFilelen, &vioSeek, &vioRead, &vioWrite, &vioTell,
            };
            MemoryBuffer buffer;
            {
                SndfileHandle file(vio, &buffer, SFM_WRITE,
                                   SF_FORMAT_WAV | SF_FORMAT_FLOAT, channels, sampleRate);
                if (file.error() != SF_ERR_NO_ERROR) {
                    return DsspApi::internalError(
                        QStringLiteral("Failed to create WAV encoder: %1")
                            .arg(QString::fromUtf8(file.strError())));
                }
                const auto written =
                    file.write(audio.data(), static_cast<sf_count_t>(audio.size()));
                if (written != static_cast<sf_count_t>(audio.size())) {
                    return DsspApi::internalError(
                        QStringLiteral("Failed to write WAV data: %1")
                            .arg(QString::fromUtf8(file.strError())));
                }
                // The destructor (sf_close) patches the RIFF/data chunk sizes
                // before we snapshot the buffer below.
            }
            out = QByteArray(buffer.data.data(), static_cast<qsizetype>(buffer.data.size()));
            return std::nullopt;
        }

        // === Duration mapping output ===
        std::optional<DsspApi::Problem> buildDurationOutput(
            const QList<ApiNote> &notes, const BuildWordsResult &built,
            const std::vector<double> &durations, QJsonArray &outNotes) {
            size_t expectedPhonemes = 0;
            for (const auto &targets : built.mapping)
                expectedPhonemes += targets.size();
            if (durations.size() != expectedPhonemes) {
                return DsspApi::internalError(
                    QStringLiteral("Duration result count mismatch: expected %1, got %2")
                        .arg(static_cast<qulonglong>(expectedPhonemes))
                        .arg(static_cast<qulonglong>(durations.size())));
            }

            QVector<QVector<double>> notePhonemeStarts(notes.size());
            for (int i = 0; i < notes.size(); ++i)
                notePhonemeStarts[i] = QVector<double>(notes.at(i).phonemes.size(), 0.0);

            size_t durationIndex = 0;
            for (size_t w = 0; w < built.words.size(); ++w) {
                const auto &word = built.words[w];
                const auto &targets = built.mapping[w];
                double cursor = 0;
                for (const auto &target : targets) {
                    if (target.valid) {
                        const auto start = word.start + cursor - target.noteStart;
                        if (target.noteIndex >= 0 && target.noteIndex < notePhonemeStarts.size() &&
                            target.phonemeIndex >= 0 &&
                            target.phonemeIndex < notePhonemeStarts[target.noteIndex].size()) {
                            notePhonemeStarts[target.noteIndex][target.phonemeIndex] = start;
                        }
                    }
                    cursor += durations[durationIndex++];
                }
            }

            for (int i = 0; i < notes.size(); ++i) {
                QJsonArray phonemes;
                for (const auto start : notePhonemeStarts[i])
                    phonemes.append(QJsonObject{{QStringLiteral("start"), start}});
                outNotes.append(QJsonObject{{QStringLiteral("phonemes"), phonemes}});
            }
            return std::nullopt;
        }

        std::optional<DsspApi::Problem> requireEngineReady() {
            if (!SynthrtEngine::instance().sessionReady()) {
                return DsspApi::internalError(
                    QStringLiteral("Inference engine is not ready"));
            }
            return std::nullopt;
        }

    } // namespace

    DsspApi::Result duration(const QJsonObject &body) {
        if (auto problem = requireEngineReady())
            return DsspApi::Result::fail(*problem);

        ResolvedContext context;
        if (auto problem = resolveContext(body, context))
            return DsspApi::Result::fail(*problem);
        ParsedNotesResult parsed;
        if (auto problem = parseNotesInput(body, /*withPhonemeStarts=*/false, context, parsed))
            return DsspApi::Result::fail(*problem);

        StageInputs stages;
        if (auto problem = acquireStages(context, stages))
            return DsspApi::Result::fail(*problem);
        if (auto problem = fillMappedSpeakers(context.singers, stages.durationSpeakerMapping))
            return DsspApi::Result::fail(*problem);

        BuildWordsResult built;
        if (auto problem = buildWords(parsed.notes, mappedSpeakerNames(context.singers),
                                      context.mix, context.mixSampleRate, built))
            return DsspApi::Result::fail(*problem);

        if (built.words.empty()) {
            // No words: empty output for every note.
            QJsonArray outNotes;
            for (int i = 0; i < parsed.notes.size(); ++i) {
                QJsonArray phonemes;
                for (int k = 0; k < parsed.notes.at(i).phonemes.size(); ++k)
                    phonemes.append(QJsonObject{{QStringLiteral("start"), 0.0}});
                outNotes.append(QJsonObject{{QStringLiteral("phonemes"), phonemes}});
            }
            return DsspApi::Result::ok(QJsonObject{
                {QStringLiteral("state"), QStringLiteral("COMPLETE")},
                {QStringLiteral("output"), QJsonObject{{QStringLiteral("notes"), outNotes}}},
            });
        }

        SynthesisSlotGuard slotGuard;

        auto input = srt::core::NO<Dur::DurationStartInput>::create();
        input->duration = context.pieceDuration;
        input->words.reserve(built.words.size());
        for (const auto &word : built.words)
            input->words.push_back(word.word);

        srt::core::NO<Dur::DurationResult> result;
        if (auto problem =
                runStage(stages, ds::infer::StageKind::Duration, input, "duration", result))
            return DsspApi::Result::fail(*problem);

        QJsonArray outNotes;
        if (auto problem = buildDurationOutput(parsed.notes, built, result->durations, outNotes))
            return DsspApi::Result::fail(*problem);

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("COMPLETE")},
            {QStringLiteral("output"), QJsonObject{{QStringLiteral("notes"), outNotes}}},
        });
    }

    DsspApi::Result parameter(const QJsonObject &body) {
        if (auto problem = requireEngineReady())
            return DsspApi::Result::fail(*problem);

        ResolvedContext context;
        if (auto problem = resolveContext(body, context))
            return DsspApi::Result::fail(*problem);
        ParsedNotesResult parsed;
        if (auto problem = parseNotesInput(body, /*withPhonemeStarts=*/true, context, parsed))
            return DsspApi::Result::fail(*problem);

        QJsonObject input;
        DsspApi::readObject(body, QStringLiteral("input"), input);
        QJsonObject parameters;
        if (!DsspApi::readObject(input, QStringLiteral("parameters"), parameters)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input.parameters\"")));
        }

        // Plan: which stages to run, driven by parameter retake markers.
        bool needsPitch = false;
        QSet<QString> varianceRetakes;
        for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
            const auto &id = it.key();
            if (!it.value().isObject())
                continue;
            const auto paramObj = it.value().toObject();
            if (!paramObj.contains(QStringLiteral("retake")))
                continue;
            if (id == QStringLiteral("pitch")) {
                needsPitch = true;
            } else if (varianceIds.contains(id)) {
                varianceRetakes.insert(id);
            } else {
                return DsspApi::Result::fail(DsspApi::invalidParameter(
                    id, QStringLiteral("retake_not_supported"),
                    QStringLiteral("Parameter \"%1\" does not support retake").arg(id)));
            }
        }

        QJsonObject outputParams;
        if (!needsPitch && varianceRetakes.isEmpty()) {
            return DsspApi::Result::ok(QJsonObject{
                {QStringLiteral("state"), QStringLiteral("COMPLETE")},
                {QStringLiteral("output"),
                 QJsonObject{{QStringLiteral("parameters"), outputParams}}},
            });
        }

        StageInputs stages;
        if (auto problem = acquireStages(context, stages))
            return DsspApi::Result::fail(*problem);
        if (auto problem = fillMappedSpeakers(context.singers, stages.pitchSpeakerMapping))
            return DsspApi::Result::fail(*problem);

        BuildWordsResult built;
        if (auto problem = buildWords(parsed.notes, mappedSpeakerNames(context.singers),
                                      context.mix, context.mixSampleRate, built))
            return DsspApi::Result::fail(*problem);

        SynthesisSlotGuard slotGuard;

        const auto dynamicSpeakers =
            buildDynamicSpeakers(context.singers, context.mix, context.mixSampleRate);
        std::optional<Co::InputParameterInfo> computedPitch;

        if (needsPitch) {
            if (!parameters.contains(QStringLiteral("expressiveness"))) {
                return DsspApi::Result::fail(DsspApi::invalidParameter(
                    QStringLiteral("expressiveness"), QStringLiteral("missing"),
                    QStringLiteral("Missing expressiveness parameter")));
            }
            if (!parameters.contains(QStringLiteral("pitch"))) {
                return DsspApi::Result::fail(DsspApi::invalidParameter(
                    QStringLiteral("pitch"), QStringLiteral("missing"),
                    QStringLiteral("Missing pitch parameter")));
            }
            Co::InputParameterInfo expressiveness;
            if (auto problem = buildParameter(
                    parameters.value(QStringLiteral("expressiveness")).toObject(),
                    QStringLiteral("expressiveness"), false, {1000.0}, expressiveness))
                return DsspApi::Result::fail(*problem);
            Co::InputParameterInfo pitch;
            if (auto problem = buildParameter(
                    parameters.value(QStringLiteral("pitch")).toObject(), QStringLiteral("pitch"),
                    true, {0.0}, pitch))
                return DsspApi::Result::fail(*problem);

            auto input = srt::core::NO<Pit::PitchStartInput>::create();
            input->duration = context.pieceDuration;
            for (const auto &word : built.words)
                input->words.push_back(word.word);
            input->parameters = {expressiveness, pitch};
            input->speakers = dynamicSpeakers;
            input->steps = context.steps;

            srt::core::NO<Pit::PitchResult> result;
            if (auto problem =
                    runStage(stages, ds::infer::StageKind::Pitch, input, "pitch", result))
                return DsspApi::Result::fail(*problem);

            computedPitch = Co::InputParameterInfo{Co::Tags::Pitch, result->pitch,
                                                   result->interval, std::nullopt};
            QJsonArray values;
            for (const auto value : result->pitch)
                values.append(value * 100.0);
            outputParams.insert(
                QStringLiteral("pitch"),
                QJsonObject{
                    {QStringLiteral("values"), values},
                    {QStringLiteral("sample_rate"), 1.0 / result->interval},
                });
        }

        if (!varianceRetakes.isEmpty()) {
            if (auto problem = fillMappedSpeakers(context.singers,
                                                  stages.varianceSpeakerMapping))
                return DsspApi::Result::fail(*problem);

            std::vector<Co::InputParameterInfo> varianceParams;
            if (computedPitch) {
                varianceParams.push_back(*computedPitch);
            } else {
                if (!parameters.contains(QStringLiteral("pitch"))) {
                    return DsspApi::Result::fail(DsspApi::invalidParameter(
                        QStringLiteral("pitch"), QStringLiteral("missing"),
                        QStringLiteral("Missing pitch parameter")));
                }
                Co::InputParameterInfo pitch;
                if (auto problem = buildParameter(
                        parameters.value(QStringLiteral("pitch")).toObject(),
                        QStringLiteral("pitch"), false, {0.0}, pitch))
                    return DsspApi::Result::fail(*problem);
                varianceParams.push_back(std::move(pitch));
            }
            for (const auto &id : varianceIds) {
                if (!varianceRetakes.contains(id))
                    continue;
                Co::InputParameterInfo parameter;
                if (auto problem = buildParameter(parameters.value(id).toObject(), id, true, {0.0},
                                                  parameter))
                    return DsspApi::Result::fail(*problem);
                varianceParams.push_back(std::move(parameter));
            }

            auto input = srt::core::NO<Var::VarianceStartInput>::create();
            input->duration = context.pieceDuration;
            for (const auto &word : built.words)
                input->words.push_back(word.word);
            input->parameters = std::move(varianceParams);
            input->speakers = dynamicSpeakers;
            input->steps = context.steps;

            srt::core::NO<Var::VarianceResult> result;
            if (auto problem = runStage(stages, ds::infer::StageKind::Variance, input,
                                        "variance", result))
                return DsspApi::Result::fail(*problem);

            for (const auto &id : varianceIds) {
                if (!varianceRetakes.contains(id))
                    continue;
                const auto *spec = findSpec(id);
                bool found = false;
                for (const auto &prediction : result->predictions) {
                    if (prediction.tag.name() == spec->tag.name()) {
                        QJsonArray values;
                        for (const auto value : prediction.values)
                            values.append(spec->inverseTransform(value));
                        outputParams.insert(
                            id, QJsonObject{
                                    {QStringLiteral("values"), values},
                                    {QStringLiteral("sample_rate"), 1.0 / prediction.interval},
                                });
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    outputParams.insert(
                        id,
                        QJsonObject{
                            {QStringLiteral("values"), QJsonArray{0}},
                            {QStringLiteral("sample_rate"),
                             parameters.value(id).toObject()
                                 .value(QStringLiteral("sample_rate"))
                                 .toDouble()},
                        });
                }
            }
        }

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("COMPLETE")},
            {QStringLiteral("output"),
             QJsonObject{{QStringLiteral("parameters"), outputParams}}},
        });
    }

    DsspApi::Result audio(const QJsonObject &body) {
        if (auto problem = requireEngineReady())
            return DsspApi::Result::fail(*problem);

        ResolvedContext context;
        if (auto problem = resolveContext(body, context))
            return DsspApi::Result::fail(*problem);
        ParsedNotesResult parsed;
        if (auto problem = parseNotesInput(body, /*withPhonemeStarts=*/true, context, parsed))
            return DsspApi::Result::fail(*problem);

        QJsonObject input;
        DsspApi::readObject(body, QStringLiteral("input"), input);
        QJsonObject parameters;
        if (!DsspApi::readObject(input, QStringLiteral("parameters"), parameters)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input.parameters\"")));
        }

        std::vector<Co::InputParameterInfo> audioParameters;
        audioParameters.reserve(audioParameterIds.size());
        for (const auto &id : audioParameterIds) {
            if (!parameters.contains(id)) {
                return DsspApi::Result::fail(DsspApi::invalidParameter(
                    id, QStringLiteral("missing"),
                    QStringLiteral("Missing %1 parameter").arg(id)));
            }
            Co::InputParameterInfo parameter;
            if (auto problem = buildParameter(parameters.value(id).toObject(), id, false, {0.0},
                                              parameter))
                return DsspApi::Result::fail(*problem);
            audioParameters.push_back(std::move(parameter));
        }

        StageInputs stages;
        if (auto problem = acquireStages(context, stages))
            return DsspApi::Result::fail(*problem);
        if (auto problem = fillMappedSpeakers(context.singers, stages.acousticSpeakerMapping))
            return DsspApi::Result::fail(*problem);

        BuildWordsResult built;
        if (auto problem = buildWords(parsed.notes, mappedSpeakerNames(context.singers),
                                      context.mix, context.mixSampleRate, built))
            return DsspApi::Result::fail(*problem);

        SynthesisSlotGuard slotGuard;

        const auto dynamicSpeakers =
            buildDynamicSpeakers(context.singers, context.mix, context.mixSampleRate);

        // Acoustic
        auto acousticInput = srt::core::NO<Ac::AcousticStartInput>::create();
        acousticInput->duration = context.pieceDuration;
        for (const auto &word : built.words)
            acousticInput->words.push_back(word.word);
        acousticInput->parameters = audioParameters;
        acousticInput->speakers = dynamicSpeakers;
        acousticInput->depth = context.depth;
        acousticInput->steps = context.steps;

        srt::core::NO<Ac::AcousticResult> acousticResult;
        if (auto problem = runStage(stages, ds::infer::StageKind::Acoustic, acousticInput,
                                    "acoustic", acousticResult))
            return DsspApi::Result::fail(*problem);

        // Vocoder
        auto vocoderInput = srt::core::NO<Vo1::VocoderStartInput>::create();
        vocoderInput->mel = acousticResult->mel;
        vocoderInput->f0 = acousticResult->f0;

        auto loadExp = stages.handle->load(ds::infer::StageKind::Vocoder);
        if (!loadExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to load vocoder model: %1")
                    .arg(QString::fromUtf8(loadExp.error().message()))));
        }
        auto inferenceVocoder = loadExp.take();
        auto startExp = inferenceVocoder->start(vocoderInput);
        if (!startExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to start vocoder inference: %1")
                    .arg(QString::fromUtf8(startExp.error().message()))));
        }
        auto taskResult = startExp.take();
        if (!taskResult) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Vocoder result type mismatch or null result")));
        }
        if (!taskResult->error.ok()) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Vocoder inference failed: %1")
                    .arg(QString::fromUtf8(taskResult->error.message()))));
        }

        std::vector<float> audioData;
        int sampleRate = 44100;
        int channels = 1;
        if (auto result = taskResult.as<Vo1::VocoderResult>()) {
            const auto &raw = result->audioData;
            const auto *samples = reinterpret_cast<const float *>(raw.data());
            audioData.assign(samples, samples + raw.size() / sizeof(float));
        } else if (auto result = taskResult.as<Vo2::VocoderResult>()) {
            audioData = result->audioData;
            sampleRate = result->sampleRate > 0 ? result->sampleRate : 44100;
            channels = result->channels > 0 ? result->channels : 1;
        } else {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Unknown vocoder result type")));
        }
        if (audioData.empty()) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Vocoder produced empty audio")));
        }

        QByteArray wav;
        if (auto problem = encodeWav(audioData, sampleRate, channels, wav))
            return DsspApi::Result::fail(*problem);

        const auto audioUrl = QStringLiteral("data:audio/wav;base64,%1")
                                  .arg(DsspApi::base64Encode(wav));
        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("COMPLETE")},
            {QStringLiteral("output"), QJsonObject{{QStringLiteral("audio_url"), audioUrl}}},
        });
    }

} // namespace DsspSynthesis
