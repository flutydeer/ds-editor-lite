#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Tasks/PhonemeDistribution.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    PhonemeName phone(const char *name, const bool onset) {
        PhonemeName result;
        result.language = "eng";
        result.name = QString::fromLatin1(name);
        result.isOnset = onset;
        return result;
    }

    QList<PhonemeName> internationalPhones() {
        return {
            phone("ih", true), phone("n", false), phone("t", true), phone("er", true),
            phone("n", false), phone("ae", true), phone("sh", true),
        };
    }

    void configureNote(Note &note, const int start, const QString &lyric) {
        note.setLocalStart(start);
        note.setLength(480);
        note.setLyric(lyric);
    }

    void testRanges() {
        const auto phones = internationalPhones();
        const auto ranges = PhonemeDistribution::phonemeRangesForNotes(
            {"international+", "+", "++"}, phones);
        expect(ranges.size() == 3, "one range is returned for every note");
        expect(ranges.at(0).start == 0 && ranges.at(0).count == 3,
               "the word trailing plus assigns two onset groups to the root");
        expect(ranges.at(1).start == 3 && ranges.at(1).count == 2,
               "the first plus note consumes one onset group");
        expect(ranges.at(2).start == 5 && ranges.at(2).count == 2,
               "the last plus note greedily consumes all remaining groups");

        const auto withSlur = PhonemeDistribution::phonemeRangesForNotes(
            {"international+", "-", "+", "++"}, phones);
        expect(withSlur.at(1).count == 0 && withSlur.at(2).start == 3,
               "slurs do not consume an onset group");

        const auto single =
            PhonemeDistribution::phonemeRangesForNotes({"international+"}, phones);
        expect(single.at(0).count == phones.size(),
               "a group without allocation notes retains every phoneme on the root");
    }

    void testStorageAndInferenceRoundTrip() {
        const auto phones = internationalPhones();
        QList<NoteInferenceSnapshot> snapshots(3);
        snapshots[0].lyric = "international+";
        snapshots[1].lyric = "+";
        snapshots[2].lyric = "++";

        QList<PhonemeNameResult> fetched(3);
        fetched[0].phonemeNames = phones;
        fetched[0].success = true;
        fetched[1].phonemeNames = {phone("stale", true)};
        fetched[2].phonemeNames = {phone("stale", true)};
        PhonemeDistribution::keepPhonemesOnGroupRoots(snapshots, fetched);
        expect(fetched.at(0).phonemeNames == phones,
               "the fetched sequence stays intact on the group root");
        expect(fetched.at(1).phonemeNames.isEmpty() && fetched.at(2).phonemeNames.isEmpty(),
               "allocation notes never retain fetched phonemes");

        Note root;
        configureNote(root, 0, "international+");
        Phonemes rootPhonemes;
        rootPhonemes.nameSeq.edited = phones;
        rootPhonemes.offsetSeq.edited = {-100, 0, 100, 500, 900, 1000, 1500};
        root.setPhonemes(rootPhonemes);
        Note plus;
        configureNote(plus, 480, "+");
        Note doublePlus;
        configureNote(doublePlus, 960, "++");

        QList<InferInputNote> inferenceNotes{
            InferInputNote(root), InferInputNote(plus), InferInputNote(doublePlus)};
        const QStringList lyrics{root.lyric(), plus.lyric(), doublePlus.lyric()};
        const Timeline timeline;
        PhonemeDistribution::distributeForInference(lyrics, inferenceNotes, timeline, 0);

        expect(inferenceNotes.at(0).phonemeNames.size() == 3 &&
                   inferenceNotes.at(1).phonemeNames.size() == 2 &&
                   inferenceNotes.at(2).phonemeNames.size() == 2,
               "the inference snapshot receives the onset-based allocation");
        expect(inferenceNotes.at(0).phonemeOffsets == QList<int>({-100, 0, 100}) &&
                   inferenceNotes.at(1).phonemeOffsets == QList<int>({0, 400}) &&
                   inferenceNotes.at(2).phonemeOffsets == QList<int>({0, 500}),
               "stored group offsets become note-relative inference offsets");

        const auto stored =
            PhonemeDistribution::collectForStorage(lyrics, inferenceNotes, timeline, 0);
        expect(stored.at(0) == rootPhonemes.offsetSeq.edited,
               "distributed offsets merge back into the complete root sequence");
        expect(stored.at(1).isEmpty() && stored.at(2).isEmpty(),
               "allocation notes receive no persisted offsets");
    }

    void testEditingEligibility() {
        Note word;
        configureNote(word, 0, "word");
        Note slur;
        configureNote(slur, 480, "-");
        Note plus;
        configureNote(plus, 960, "++");
        expect(word.canEditPhonemes(), "ordinary notes can edit phonemes");
        expect(!slur.canEditPhonemes() && !plus.canEditPhonemes(),
               "slur and allocation notes cannot edit phonemes");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testRanges();
    testStorageAndInferenceRoundTrip();
    testEditingEligibility();
    if (failures == 0)
        QTextStream(stdout) << "All PhonemeDistribution tests passed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
