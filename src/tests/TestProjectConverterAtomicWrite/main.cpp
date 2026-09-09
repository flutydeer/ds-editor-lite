#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
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

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {


    using TestSupport::expect;

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

class ProjectConverterAtomicWriteTests final : public QObject {
    Q_OBJECT

private slots:

    void dspxAtomicWrite() {
        testDspxAtomicWrite();
    }

    void midiAtomicWrite() {
        testMidiAtomicWrite();
    }

    void dspxTimeSignatureProjectionValidation() {
        testDspxTimeSignatureProjectionValidation();
    }

    void dspxRoundTripPreservesEditedPhrase() {
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
        const SingerInfo singer(
            {QStringLiteral("voice"), QStringLiteral("fixture-package"), QVersionNumber(1, 2)},
            QStringLiteral("Fixture voice"), {soft, strong});
        SpeakerMixModel::SpeakerMixData mix;
        mix.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
        mix.sources = {{soft}, {strong}};
        mix.fixedWeights = {0.25};
        mix.dynamicKeyframes = {
            {0,   {0.25}},
            {480, {0.8} }
        };
        clip->setOwnVoiceContext(singer, soft, mix);

        DspxProjectConverter converter;
        QString error;
        QVERIFY2(converter.save(path, &original, error), qPrintable(error));
        AppModel reopened;
        QVERIFY2(converter.load(path, &reopened, error, ImportMode::NewProject), qPrintable(error));
        QVERIFY(reopened.timeline() == original.timeline());
        QCOMPARE(reopened.tracks().size(), 1);
        const auto *restoredTrack = reopened.tracks().first();
        QCOMPARE(restoredTrack->name(), track->name());
        QCOMPARE(restoredTrack->clips().count(), 1);
        const auto *restoredClip =
            dynamic_cast<const SingingClip *>(*restoredTrack->clips().begin());
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
        QCOMPARE(restoredMix.sources.size(), 2);
        QCOMPARE(restoredMix.sources.first().speaker.id(), soft.id());
        QCOMPARE(restoredMix.sources.last().speaker.id(), strong.id());
        QCOMPARE(restoredMix.dynamicKeyframes.size(), 2);
        QCOMPARE(restoredMix.dynamicKeyframes.first().tick, 0);
        QCOMPARE(restoredMix.dynamicKeyframes.last().tick, 480);
        QCOMPARE(restoredMix.dynamicKeyframes.first().weights, QVector<double>({0.25}));
        QCOMPARE(restoredMix.dynamicKeyframes.last().weights, QVector<double>({0.8}));
    }

    void audioPublicationOverwrite() {
        testAudioPublicationOverwrite();
    }

    void audioPublicationNoClobber() {
        testAudioPublicationNoClobber();
    }
};

QTEST_GUILESS_MAIN(ProjectConverterAtomicWriteTests)
#include "main.moc"
