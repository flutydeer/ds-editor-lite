#include "tst_document_io.h"

#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/DspxPhonemeCompat.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <opendspx/model.h>

#include "Modules/Audio/AudioFilePublisher.h"

#include <QCoreApplication>
#include <QtTest/QTest>
#include "../TestSupport/TestAssertions.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <functional>
#include <limits>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {


    using TestSupport::expect;

    class SingerMetadataConverter final : public DspxProjectConverter {
    public:
        SingerInfo availableSinger;

    protected:
        SingerInfo resolveSinger(const SingerIdentifier &identifier) const override {
            return availableSinger.identifier() == identifier ? availableSinger : SingerInfo{};
        }
    };

    bool writeFile(const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(data) == data.size();
    }

    QByteArray readFile(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    QStringList directoryEntries(const QString &path) {
        return QDir(path).entryList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
    }

#ifdef Q_OS_WIN
    class ReplacementBlocker final {
    public:
        explicit ReplacementBlocker(const QString &path) {
            m_handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        }

        ~ReplacementBlocker() {
            if (valid())
                CloseHandle(m_handle);
        }

        Q_DISABLE_COPY_MOVE(ReplacementBlocker)

        [[nodiscard]] bool valid() const {
            return m_handle != INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE m_handle = INVALID_HANDLE_VALUE;
    };
#endif

    template <typename Save>
    void verifyAtomicReplacement(const QString &label, const QString &suffix, Save save,
                                 const std::function<bool(const QByteArray &)> &validOutput) {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("project.") + suffix);
        const QByteArray original = QByteArrayLiteral("existing-file-must-survive");
        expect(directory.isValid() && writeFile(path, original),
               label + QStringLiteral(" fixture must be created"));
        if (!directory.isValid() || readFile(path) != original)
            return;

#ifdef Q_OS_WIN
        {
            ReplacementBlocker blocker(path);
            expect(blocker.valid(), label + QStringLiteral(" replacement blocker must open"));
            const auto entriesBefore = directoryEntries(directory.path());
            QString error;
            const auto saved = save(path, error);
            expect(!saved, label + QStringLiteral(" must report a blocked atomic commit"));
            expect(readFile(path) == original,
                   label + QStringLiteral(" failed commit must preserve the existing file"));
            expect(directoryEntries(directory.path()) == entriesBefore,
                   label + QStringLiteral(" failed commit must remove its temporary file"));
        }
#endif

        QString error;
        const auto saved = save(path, error);
        const auto output = readFile(path);
        expect(saved && error.isEmpty(), label + QStringLiteral(" replacement must succeed"));
        expect(output != original && validOutput(output),
               label + QStringLiteral(" replacement must contain a valid new document"));
        expect(directoryEntries(directory.path()) == QStringList{QFileInfo(path).fileName()},
               label + QStringLiteral(" success must leave only the committed target"));
    }

    void testDspxAtomicWrite() {
        AppModel model;
        model.newProject();
        DspxProjectConverter converter;
        verifyAtomicReplacement(
            QStringLiteral("DSPX"), QStringLiteral("dspx"),
            [&converter, &model](const QString &path, QString &error) {
                return converter.save(path, &model, error);
            },
            [](const QByteArray &output) {
                return output.trimmed().startsWith('{') && output.contains("\"content\"");
            });
    }

    void testMidiAtomicWrite() {
        AppModel model;
        model.newProject();
        MidiConverter converter;
        verifyAtomicReplacement(
            QStringLiteral("MIDI"), QStringLiteral("mid"),
            [&converter, &model](const QString &path, QString &error) {
                return converter.save(path, &model, error);
            },
            [](const QByteArray &output) { return output.startsWith("MThd"); });
    }

    void testDspxTimeSignatureProjectionValidation() {
        opendspx::Model project;
        project.content.timeline.tempos.push_back({0, 120.0});
        project.content.timeline.timeSignatures.push_back(
            {0, (std::numeric_limits<int>::max)(), 1});
        project.content.timeline.timeSignatures.push_back({1, 4, 4});

        AppModel model;
        LoopSettings loopSettings;
        QString error;
        DspxProjectConverter converter;
        expect(!converter.loadParsedProject(project, &model, loopSettings, error,
                                            ImportMode::NewProject) &&
                   !error.isEmpty(),
               QStringLiteral("DSPX load must reject out-of-range time signatures"));
    }

    void testAudioPublicationOverwrite() {
        QTemporaryDir directory;
        const auto target = directory.filePath(QStringLiteral("audio.wav"));
        const auto temporary = directory.filePath(QStringLiteral("audio.exporting"));
        const auto fixtureReady = directory.isValid() &&
                                  writeFile(target, QByteArrayLiteral("original")) &&
                                  writeFile(temporary, QByteArrayLiteral("replacement"));
        expect(fixtureReady, QStringLiteral("audio publication fixtures must be created"));
        if (!fixtureReady)
            return;

        const auto result = Audio::Internal::publishAudioFiles(
            {
                {target, temporary}
        },
            true);
        expect(result.succeeded() && readFile(target) == QByteArrayLiteral("replacement") &&
                   !QFileInfo::exists(temporary),
               QStringLiteral("overwrite publication must replace the target atomically"));
    }

    void testAudioPublicationNoClobber() {
        QTemporaryDir directory;
        const auto target = directory.filePath(QStringLiteral("audio.wav"));
        const auto temporary = directory.filePath(QStringLiteral("audio.exporting"));
        const auto fixtureReady = directory.isValid() &&
                                  writeFile(target, QByteArrayLiteral("external")) &&
                                  writeFile(temporary, QByteArrayLiteral("replacement"));
        expect(fixtureReady,
               QStringLiteral("reject-mode audio publication fixtures must be created"));
        if (!fixtureReady)
            return;

        const auto result = Audio::Internal::publishAudioFiles(
            {
                {target, temporary}
        },
            false);
        expect(!result.succeeded() && result.failedTarget == target &&
                   readFile(target) == QByteArrayLiteral("external") &&
                   QFileInfo::exists(temporary),
               QStringLiteral("reject-mode publication must preserve an existing target"));
    }
}

void DocumentIOTests::dspxPhonemeInterchangeRespectsExternalChanges() {
    PhonemeName onset;
    onset.language = QStringLiteral("eng");
    onset.name = QStringLiteral("l");
    onset.isOnset = true;
    PhonemeName vowel;
    vowel.language = QStringLiteral("eng");
    vowel.name = QStringLiteral("a");
    auto editedOnset = onset;
    editedOnset.name = QStringLiteral("m");
    Phonemes source;
    source.nameSeq.original = {onset, vowel};
    source.nameSeq.edited = {editedOnset, vowel};
    source.offsetSeq.original = {-40, 0};
    source.offsetSeq.edited = {-25, 15};
    opendspx::Phonemes standard;
    QJsonObject workspace;
    DspxPhonemeCompat::encode(source, standard, workspace);
    QCOMPARE(DspxPhonemeCompat::decode(standard, &workspace).serialize(), source.serialize());

    standard.original.front().token = "r";
    standard.edited.front().token = "n";
    standard.edited.front().start = -20;
    const auto external = DspxPhonemeCompat::decode(standard, &workspace);
    auto externalOriginal = onset;
    externalOriginal.name = QStringLiteral("r");
    auto externalEdited = onset;
    externalEdited.name = QStringLiteral("n");
    QCOMPARE(external.nameSeq.original, (QList<PhonemeName>{externalOriginal, vowel}));
    QCOMPARE(external.nameSeq.edited, (QList<PhonemeName>{externalEdited, vowel}));
    QCOMPARE(external.offsetSeq.original, source.offsetSeq.original);
    QCOMPARE(external.offsetSeq.edited, (QList<int>{-20, 15}));
    QCOMPARE(DspxPhonemeCompat::decode(standard).serialize(), external.serialize());

    standard.edited.clear();
    const auto reset = DspxPhonemeCompat::decode(standard, &workspace);
    QCOMPARE(reset.nameSeq.original, external.nameSeq.original);
    QCOMPARE(reset.offsetSeq.original, source.offsetSeq.original);
    QVERIFY(!reset.nameSeq.isEdited());
    QVERIFY(!reset.offsetSeq.isEdited());

    workspace.remove(QStringLiteral("dspxSnapshot"));
    QCOMPARE(DspxPhonemeCompat::decode({}, &workspace).serialize(), source.serialize());
    QCOMPARE(DspxPhonemeCompat::decode(standard, &workspace).serialize(), reset.serialize());
}

void DocumentIOTests::midiExportPreservesProjectTimingAndOptionalMetadata_data() {
    QTest::addColumn<bool>("metadata");
    QTest::newRow("lyrics-and-timeline") << true;
    QTest::newRow("notes-only") << false;
}

void DocumentIOTests::midiExportPreservesProjectTimingAndOptionalMetadata() {
    QFETCH(bool, metadata);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppModel model;
    auto timeline = model.timeline();
    timeline.setTempos({
        {0,    90 },
        {1920, 135}
    });
    timeline.setTimeSignatures({
        {0, 3, 4}
    });
    model.setTimeline(timeline);
    auto *track = new Track;
    track->setName(QStringLiteral("乐句"));
    const QList<int> starts{480, 2400};
    const QStringList lyrics{QStringLiteral("你好"), QStringLiteral("world")};
    for (int i = 0; i < starts.size(); ++i) {
        auto *clip = new SingingClip;
        clip->setStart(starts[i]);
        clip->setLength(960);
        clip->setClipStart(i * 60);
        clip->setClipLen(960 - i * 60);
        auto *note = new Note(clip);
        note->setLocalStart(120);
        note->setLength(240);
        note->setKeyIndex(60 + i * 7);
        note->setLyric(lyrics[i]);
        clip->insertNote(note);
        track->insertClip(clip);
    }
    auto *audio = new AudioClip;
    audio->setStart(4800);
    audio->setLength(960);
    audio->setClipLen(960);
    audio->setPath(directory.filePath(QStringLiteral("audio-is-not-midi.wav")));
    track->insertClip(audio);
    QVERIFY(model.appendTrack(track));
    const auto before = model.serialize();
    MidiConverter converter;
    const auto path = directory.filePath(QStringLiteral("导出.mid"));
    QString error;
    QVERIFY2(converter.save(path, &model, error,
                            {.includeTempo = metadata,
                             .includeTimeSignatures = metadata,
                             .includeLyrics = metadata}),
             qPrintable(error));
    QCOMPARE(model.serialize(), before);
    const auto parsed = MidiFileParser::parse(path);
    QVERIFY2(parsed.valid, qPrintable(parsed.errorMessage));
    std::vector<opendspx::MidiIntermediateData::Note> notes;
    for (const auto &midiTrack : parsed.mediate.tracks()) {
        if (midiTrack.notes.empty())
            continue;
        QCOMPARE(QString::fromStdString(midiTrack.title), track->name());
        notes.insert(notes.end(), midiTrack.notes.begin(), midiTrack.notes.end());
    }
    QCOMPARE(notes.size(), size_t(2));
    std::sort(notes.begin(), notes.end(), [](const auto &left, const auto &right) {
        return left.noteOnTick < right.noteOnTick;
    });
    for (int i = 0; i < starts.size(); ++i) {
        QCOMPARE(notes[i].noteOnTick, starts[i] + 120);
        QCOMPARE(notes[i].length, 240);
        QCOMPARE(notes[i].key, 60 + i * 7);
        QCOMPARE(QString::fromStdString(notes[i].lyric), metadata ? lyrics[i] : QString{});
    }
    const auto tempos = parsed.mediate.tempos();
    const auto signatures = parsed.mediate.timeSignatures();
    if (!metadata) {
        QVERIFY(tempos.empty());
        QVERIFY(signatures.empty());
    }
    const auto includesTempo = [&](int tick, double bpm) {
        return std::any_of(tempos.begin(), tempos.end(), [&](const auto &tempo) {
            return tempo.tick == tick && std::abs(tempo.tempo - bpm) < 0.001;
        });
    };
    QCOMPARE(includesTempo(0, 90), metadata);
    QCOMPARE(includesTempo(1920, 135), metadata);
    QCOMPARE(std::any_of(signatures.begin(), signatures.end(),
                         [](const auto &signature) {
                             return signature.tick == 0 && signature.numerator == 3 &&
                                    signature.denominator == 4;
                         }),
             metadata);
}

void DocumentIOTests::dspxAtomicWrite() {
    testDspxAtomicWrite();
}

void DocumentIOTests::midiAtomicWrite() {
    testMidiAtomicWrite();
}

void DocumentIOTests::dspxTimeSignatureProjectionValidation() {
    testDspxTimeSignatureProjectionValidation();
}

void DocumentIOTests::dspxRoundTripPreservesEditedPhrase_data() {
    QTest::addColumn<bool>("removedLastSource");
    QTest::newRow("unavailable-package-preserves-content") << false;
    QTest::newRow("resolved-package-retains-remaining-ratio") << true;
}

void DocumentIOTests::dspxRoundTripPreservesEditedPhrase() {
    QFETCH(bool, removedLastSource);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("edited-phrase.dspx"));

    AppModel original;
    original.setTimeline(Timeline(
        {
            {0,    120.0},
            {1920, 90.0 }
    },
        {TimeSignature(0, 4, 4), TimeSignature(2, 3, 4)}));
    auto *track = new Track;
    track->setName(QStringLiteral("Lead / 测试"));
    track->setDefaultLanguage(QStringLiteral("eng"));
    QVERIFY(original.appendTrack(track));

    auto *clip = new SingingClip;
    clip->setName(QStringLiteral("Edited phrase"));
    clip->setStart(960);
    clip->setLength(3840);
    clip->setClipStart(120);
    clip->setClipLen(2880);
    clip->setDefaultLanguage(QStringLiteral("eng"));
    track->insertClip(clip);

    auto *note = new Note;
    note->setLocalStart(240);
    note->setLength(720);
    note->setKeyIndex(64);
    note->setLyric(QStringLiteral("世界"));
    note->setLanguage(QStringLiteral("eng"));
    note->setPronunciation(Pronunciation(QStringLiteral("world"), QStringLiteral("werld")));
    Phonemes phonemes;
    PhonemeName onset;
    onset.language = QStringLiteral("eng");
    onset.name = QStringLiteral("w");
    onset.isOnset = true;
    PhonemeName vowel;
    vowel.language = QStringLiteral("eng");
    vowel.name = QStringLiteral("er");
    phonemes.nameSeq.original = {onset, vowel};
    phonemes.nameSeq.edited = {vowel};
    phonemes.offsetSeq.original = {-40, 0};
    phonemes.offsetSeq.edited = {20};
    note->setPhonemes(phonemes);
    clip->insertNote(note);

    auto *draw = new DrawCurve;
    draw->setLocalStart(240);
    draw->step = 5;
    draw->setValues({6400, 6420, 6390, 6410});
    auto *anchor = new AnchorCurve;
    anchor->setLocalStart(480);
    auto *firstAnchor = new AnchorNode(0, 6420);
    firstAnchor->setInterpMode(AnchorNode::Linear);
    anchor->insertNode(firstAnchor);
    anchor->insertNode(new AnchorNode(240, 6500));
    clip->params.pitch.setCurves(Param::Edited, {draw, anchor}, clip);

    const SpeakerInfo soft(QStringLiteral("soft"), QStringLiteral("Soft"));
    const SpeakerInfo strong(QStringLiteral("strong"), QStringLiteral("Strong"));
    const SpeakerInfo air(QStringLiteral("air"), QStringLiteral("Air"));
    const SingerInfo singer(
        {QStringLiteral("voice"), QStringLiteral("fixture-package"), QVersionNumber(1, 2)},
        QStringLiteral("Fixture voice"),
        removedLastSource ? QList<SpeakerInfo>{soft, strong, air}
                          : QList<SpeakerInfo>{soft, strong});
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
    mix.sources = {{soft}, {strong}};
    mix.fixedWeights = {0.25};
    mix.dynamicBypassed = true;
    mix.sourcePresetId = QStringLiteral("saved-blend");
    mix.sourcePresetName = QStringLiteral("Saved blend");
    mix.sourcePresetDirty = true;
    mix.dynamicKeyframes = {
        {0,   {0.25}},
        {480, {0.8} }
    };
    clip->setOwnVoiceContext(singer, soft, mix);
    auto trackMix = mix;
    trackMix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    trackMix.dynamicKeyframes.clear();
    trackMix.dynamicBypassed = false;
    if (removedLastSource) {
        trackMix.sources.append({air});
        trackMix.fixedWeights = {0.2, 0.3};
    }
    track->setVoiceContext(singer, soft, trackMix);

    SingerMetadataConverter converter;
    if (removedLastSource) {
        converter.availableSinger = singer;
        converter.availableSinger.setSpeakers({soft, strong});
        converter.availableSinger.setResolutionState(ResolutionState::Resolved);
    }
    QString error;
    QVERIFY2(converter.save(path, &original, error), qPrintable(error));
    AppModel reopened;
    QVERIFY2(converter.load(path, &reopened, error, ImportMode::NewProject), qPrintable(error));
    QVERIFY(reopened.timeline() == original.timeline());
    QCOMPARE(reopened.tracks().size(), 1);
    const auto *restoredTrack = reopened.tracks().first();
    QCOMPARE(restoredTrack->name(), track->name());
    QCOMPARE(restoredTrack->singerInfo().identifier(), singer.identifier());
    QCOMPARE(restoredTrack->speakerMixData().mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    const auto expectedWeights = removedLastSource ? QVector<double>{0.4} : QVector<double>{0.25};
    QCOMPARE(restoredTrack->speakerMixData().fixedWeights, expectedWeights);
    QCOMPARE(restoredTrack->speakerMixData().sources.size(), 2);
    if (removedLastSource) {
        QCOMPARE(restoredTrack->singerInfo().resolutionState(), ResolutionState::Resolved);
        QCOMPARE(SpeakerMixModel::fullWeightsFromExplicitWeights(
                     restoredTrack->speakerMixData().fixedWeights),
                 (QVector<double>{0.4, 0.6}));
    }
    QCOMPARE(restoredTrack->speakerMixData().sources.last().speaker.id(), strong.id());
    QCOMPARE(restoredTrack->clips().count(), 1);
    const auto *restoredClip = dynamic_cast<const SingingClip *>(*restoredTrack->clips().begin());
    QVERIFY(restoredClip);
    QCOMPARE(restoredClip->start(), 960);
    QCOMPARE(restoredClip->clipStart(), 120);
    QCOMPARE(restoredClip->clipLen(), 2880);
    QCOMPARE(restoredClip->notes().count(), 1);
    const auto *restoredNote = *restoredClip->notes().begin();
    QCOMPARE(restoredNote->localStart(), 240);
    QCOMPARE(restoredNote->length(), 720);
    QCOMPARE(restoredNote->keyIndex(), 64);
    QCOMPARE(restoredNote->lyric(), QStringLiteral("世界"));
    QCOMPARE(restoredNote->pronunciation().edited, QStringLiteral("werld"));
    QCOMPARE(restoredNote->phonemes().nameSeq.original, phonemes.nameSeq.original);
    QCOMPARE(restoredNote->phonemes().nameSeq.edited, phonemes.nameSeq.edited);
    QCOMPARE(restoredNote->phonemes().offsetSeq.original, phonemes.offsetSeq.original);
    QCOMPARE(restoredNote->phonemes().offsetSeq.edited, phonemes.offsetSeq.edited);

    const auto &curves = restoredClip->params.pitch.curves(Param::Edited);
    QCOMPARE(curves.size(), 2);
    const auto *restoredDraw = dynamic_cast<const DrawCurve *>(curves.first());
    const auto *restoredAnchor = dynamic_cast<const AnchorCurve *>(curves.last());
    QVERIFY(restoredDraw);
    QVERIFY(restoredAnchor);
    QCOMPARE(restoredDraw->localStart(), 240);
    QCOMPARE(restoredDraw->step, 5);
    QCOMPARE(restoredDraw->values(), QList<int>({6400, 6420, 6390, 6410}));
    QCOMPARE(restoredAnchor->localStart(), 480);
    QCOMPARE(restoredAnchor->nodes().count(), 2);
    const auto nodes = restoredAnchor->nodes().toList();
    QCOMPARE(nodes.first()->pos(), 0);
    QCOMPARE(nodes.first()->value(), 6420);
    QCOMPARE(nodes.first()->interpMode(), AnchorNode::Linear);
    QCOMPARE(nodes.last()->pos(), 240);
    QCOMPARE(nodes.last()->value(), 6500);

    QVERIFY(restoredClip->ownSingerInfo().identifier() == singer.identifier());
    const auto restoredMix = restoredClip->ownSpeakerMixData();
    QCOMPARE(restoredMix.mode, SpeakerMixModel::SingerSourceMode::DynamicMix);
    QVERIFY(restoredMix.dynamicBypassed);
    QCOMPARE(restoredMix.sourcePresetId, mix.sourcePresetId);
    QCOMPARE(restoredMix.sourcePresetName, mix.sourcePresetName);
    QVERIFY(restoredMix.sourcePresetDirty);
    QCOMPARE(restoredMix.sources.size(), 2);
    QCOMPARE(restoredMix.sources.first().speaker.id(), soft.id());
    QCOMPARE(restoredMix.sources.last().speaker.id(), strong.id());
    QCOMPARE(restoredMix.dynamicKeyframes.size(), 2);
    QCOMPARE(restoredMix.dynamicKeyframes.first().tick, 0);
    QCOMPARE(restoredMix.dynamicKeyframes.last().tick, 480);
    QCOMPARE(restoredMix.dynamicKeyframes.first().weights, QVector<double>({0.25}));
    QCOMPARE(restoredMix.dynamicKeyframes.last().weights, QVector<double>({0.8}));

    if (removedLastSource) {
        const auto filteredPath = directory.filePath(QStringLiteral("filtered-phrase.dspx"));
        QVERIFY2(converter.save(filteredPath, &reopened, error), qPrintable(error));
        AppModel filteredAgain;
        QVERIFY2(converter.load(filteredPath, &filteredAgain, error, ImportMode::NewProject),
                 qPrintable(error));
        QCOMPARE(filteredAgain.tracks().first()->speakerMixData().fixedWeights, expectedWeights);
    }
}

void DocumentIOTests::dspxStandardSingerSourcesLoadNestedMixes_data() {
    QTest::addColumn<bool>("installed");
    QTest::newRow("installed-singer") << true;
    QTest::newRow("missing-singer-metadata") << false;
}

void DocumentIOTests::dspxStandardSingerSourcesLoadNestedMixes() {
    QFETCH(bool, installed);
    const SpeakerInfo soft(QStringLiteral("soft"), QStringLiteral("Soft"));
    const SpeakerInfo strong(QStringLiteral("strong"), QStringLiteral("Strong"));
    SingerInfo singer(
        {QStringLiteral("voice"), QStringLiteral("fixture-package"), QVersionNumber(1, 2)},
        QStringLiteral("Fixture voice"), {soft, strong});
    singer.setResolutionState(ResolutionState::Resolved);
    const auto source = [](const QString &speaker) {
        auto single = std::make_shared<opendspx::SingleSinger>();
        single->id = "fixture-package@1.2[voice]";
        single->extra = stdc::json::Object{
            {"speaker", speaker.toStdString()}
        };
        return single;
    };
    auto inner = std::make_shared<opendspx::MixedSinger>();
    inner->singers = {source(soft.id()), source(strong.id())};
    inner->ratio = {0.25};
    auto outer = std::make_shared<opendspx::MixedSinger>();
    outer->singers = {inner, source(strong.id())};
    outer->ratio = {0.5};
    opendspx::Sources fixed;
    fixed.category = "diffscope-synth:diffsinger";
    fixed.singers = {outer};
    auto dynamic = fixed;
    dynamic.singers = {inner, source(strong.id())};
    dynamic.mix = {
        {0,   {0.5} },
        {480, {0.75}}
    };
    opendspx::Model project;
    project.content.timeline.tempos = {
        {0, 120.0}
    };
    project.content.timeline.timeSignatures = {
        {0, 4, 4}
    };
    opendspx::Track track;
    track.name = "External singer sources";
    for (const auto &sources : {fixed, dynamic}) {
        auto phrase = std::make_shared<opendspx::SingingClip>();
        phrase->time.pos = track.clips.empty() ? 0 : 2400;
        phrase->time.length = 1920;
        phrase->time.clipLen = 1920;
        phrase->sources = sources;
        track.clips.push_back(phrase);
    }
    project.content.tracks = {track};
    SingerMetadataConverter converter;
    if (installed)
        converter.availableSinger = singer;
    AppModel loaded;
    LoopSettings loop;
    QString error;
    QVERIFY2(converter.loadParsedProject(project, &loaded, loop, error, ImportMode::NewProject),
             qPrintable(error));
    const auto verifyMixes = [&](const AppModel &model) {
        QCOMPARE(model.tracks().size(), 1);
        const auto clips = model.tracks().first()->clips().toList();
        QCOMPARE(clips.size(), 2);
        const auto *fixedClip = dynamic_cast<const SingingClip *>(clips.first());
        const auto *dynamicClip = dynamic_cast<const SingingClip *>(clips.last());
        QVERIFY(fixedClip && dynamicClip);
        for (const auto *phrase : {fixedClip, dynamicClip}) {
            QCOMPARE(phrase->ownSingerInfo().identifier(), singer.identifier());
            QCOMPARE(phrase->singerInfo().resolutionState(),
                     installed ? ResolutionState::Resolved : ResolutionState::Pending);
            const auto sources = phrase->speakerMixData().sources;
            QCOMPARE(sources.size(), 2);
            QCOMPARE(sources.first().speaker.id(), soft.id());
            QCOMPARE(sources.last().speaker.id(), strong.id());
        }
        const auto fixedMix = fixedClip->speakerMixData();
        QCOMPARE(fixedMix.mode, SpeakerMixModel::SingerSourceMode::FixedMix);
        QCOMPARE(fixedMix.fixedWeights, QVector<double>{0.125});
        const auto dynamicMix = dynamicClip->speakerMixData();
        QCOMPARE(dynamicMix.mode, SpeakerMixModel::SingerSourceMode::DynamicMix);
        QCOMPARE(dynamicMix.dynamicKeyframes.size(), 2);
        QCOMPARE(dynamicMix.dynamicKeyframes.first().tick, 0);
        QCOMPARE(dynamicMix.dynamicKeyframes.first().weights, QVector<double>{0.125});
        QCOMPARE(dynamicMix.dynamicKeyframes.last().tick, 480);
        QCOMPARE(dynamicMix.dynamicKeyframes.last().weights, QVector<double>{0.1875});
    };
    verifyMixes(loaded);
    if (QTest::currentTestFailed())
        return;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("nested-sources.dspx"));
    QVERIFY2(converter.save(path, &loaded, error), qPrintable(error));
    AppModel reopened;
    QVERIFY2(converter.load(path, &reopened, error, ImportMode::NewProject), qPrintable(error));
    verifyMixes(reopened);
}

void DocumentIOTests::audioPublicationOverwrite() {
    testAudioPublicationOverwrite();
}

void DocumentIOTests::audioPublicationNoClobber() {
    testAudioPublicationNoClobber();
}
