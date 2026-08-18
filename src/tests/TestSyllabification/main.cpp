#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Tasks/Syllabification.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

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
        const auto ranges =
            Syllabification::phonemeRangesForNotes({"international+", "+", "++"}, phones);
        expect(ranges.size() == 3, "one range is returned for every note");
        expect(ranges.at(0).start == 0 && ranges.at(0).count == 3,
               "a trailing syllabification symbol assigns two syllables to the word root");
        expect(ranges.at(1).start == 3 && ranges.at(1).count == 2,
               "the first syllabification note consumes one syllable");
        expect(ranges.at(2).start == 5 && ranges.at(2).count == 2,
               "the last syllabification note greedily consumes all remaining syllables");

        const auto withSlur =
            Syllabification::phonemeRangesForNotes({"international+", "-", "+", "++"}, phones);
        expect(withSlur.at(1).count == 0 && withSlur.at(2).start == 3,
               "slur notes do not consume a syllable");

        const auto single = Syllabification::phonemeRangesForNotes({"international+"}, phones);
        expect(single.at(0).count == phones.size(),
               "a word without syllabification notes retains every phoneme on its root");
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
        Syllabification::keepPhonemesOnWordRoots(snapshots, fetched);
        expect(fetched.at(0).phonemeNames == phones,
               "the fetched sequence stays intact on the word root");
        expect(fetched.at(1).phonemeNames.isEmpty() && fetched.at(2).phonemeNames.isEmpty(),
               "syllabification notes never retain fetched phonemes");

        Note root;
        configureNote(root, 0, "international+");
        Phonemes rootPhonemes;
        rootPhonemes.nameSeq.edited = phones;
        rootPhonemes.offsetSeq.edited = {-100, 0, 100, 500, 900, 1000, 1500};
        root.setPhonemes(rootPhonemes);
        Note syllabificationNote;
        configureNote(syllabificationNote, 480, "+");
        Note doubleSyllabificationNote;
        configureNote(doubleSyllabificationNote, 960, "++");

        QList<InferInputNote> inferenceNotes{InferInputNote(root),
                                             InferInputNote(syllabificationNote),
                                             InferInputNote(doubleSyllabificationNote)};
        const QStringList lyrics{root.lyric(), syllabificationNote.lyric(),
                                 doubleSyllabificationNote.lyric()};
        const Timeline timeline;
        Syllabification::distributeForInference(lyrics, inferenceNotes, timeline, 0);

        expect(inferenceNotes.at(0).phonemeNames.size() == 3 &&
                   inferenceNotes.at(1).phonemeNames.size() == 2 &&
                   inferenceNotes.at(2).phonemeNames.size() == 2,
               "the inference snapshot receives the syllabified phoneme ranges");
        expect(inferenceNotes.at(0).phonemeOffsets == QList<int>({-100, 0, 100}) &&
                   inferenceNotes.at(1).phonemeOffsets == QList<int>({0, 400}) &&
                   inferenceNotes.at(2).phonemeOffsets == QList<int>({0, 500}),
               "stored word offsets become note-relative inference offsets");

        const auto stored = Syllabification::collectForStorage(lyrics, inferenceNotes, timeline, 0);
        expect(stored.at(0) == rootPhonemes.offsetSeq.edited,
               "distributed offsets merge back into the complete word root sequence");
        expect(stored.at(1).isEmpty() && stored.at(2).isEmpty(),
               "syllabification notes receive no persisted offsets");
    }

    void testDetachedSyllabificationNotesStayOrphaned() {
        Note root;
        configureNote(root, 0, "international+");
        Phonemes rootPhonemes;
        rootPhonemes.nameSeq.edited = internationalPhones();
        rootPhonemes.offsetSeq.edited = {-100, 0, 100, 500, 900, 1000, 1500};
        root.setPhonemes(rootPhonemes);
        Note syllabificationNote;
        configureNote(syllabificationNote, 600, "+");
        Note doubleSyllabificationNote;
        configureNote(doubleSyllabificationNote, 1080, "++");

        QList<InferInputNote> inferenceNotes{InferInputNote(root),
                                             InferInputNote(syllabificationNote),
                                             InferInputNote(doubleSyllabificationNote)};
        const QStringList lyrics{root.lyric(), syllabificationNote.lyric(),
                                 doubleSyllabificationNote.lyric()};
        const Timeline timeline;
        Syllabification::distributeForInference(lyrics, inferenceNotes, timeline, 0);

        expect(inferenceNotes.at(0).phonemeNames == rootPhonemes.nameSeq.edited &&
                   inferenceNotes.at(1).phonemeNames.isEmpty() &&
                   inferenceNotes.at(2).phonemeNames.isEmpty(),
               "detached syllabification notes do not consume root phonemes");
        const auto stored = Syllabification::collectForStorage(lyrics, inferenceNotes, timeline, 0);
        expect(stored.at(0) == rootPhonemes.offsetSeq.edited && stored.at(1).isEmpty() &&
                   stored.at(2).isEmpty(),
               "detached syllabification notes do not merge offsets into the root");
    }

    void testEditingEligibility() {
        Note word;
        configureNote(word, 0, "word");
        Note slur;
        configureNote(slur, 480, "-");
        Note syllabificationNote;
        configureNote(syllabificationNote, 960, "++");
        expect(word.canEditPhonemes(), "ordinary notes can edit phonemes");
        expect(!slur.canEditPhonemes() && !syllabificationNote.canEditPhonemes(),
               "slur and syllabification notes cannot edit phonemes");
    }

    void testRelativeTimingChangeInvalidatesEditedOffsets() {
        auto root = new Note;
        configureNote(*root, 0, "international+");
        Phonemes rootPhonemes;
        rootPhonemes.nameSeq.edited = internationalPhones();
        rootPhonemes.offsetSeq.edited = {0, 100, 200, 500, 900, 1000, 1500};
        root->setPhonemes(rootPhonemes);
        auto syllabificationNote = new Note;
        configureNote(*syllabificationNote, 480, "+");
        auto doubleSyllabificationNote = new Note;
        configureNote(*doubleSyllabificationNote, 960, "++");
        SingingClip clip({root, syllabificationNote, doubleSyllabificationNote});

        auto previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip);
        clip.removeNote(syllabificationNote);
        syllabificationNote->setLocalStart(600);
        clip.insertNote(syllabificationNote);
        const auto resetRecords =
            SingingClipPhonemeNormalizer::normalizeEditedOffsets(clip, previousWordStates);
        expect(resetRecords.size() == 1 && resetRecords.first().note == root &&
                   !root->phonemeOffsetSeq().isEdited(),
               "moving one syllabification note invalidates root-relative edited offsets");

        SingingClipPhonemeNormalizer::restoreEditedOffsets(resetRecords);
        clip.removeNote(syllabificationNote);
        syllabificationNote->setLocalStart(480);
        clip.insertNote(syllabificationNote);
        previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip);
        for (const auto note :
             QList<Note *>{root, syllabificationNote, doubleSyllabificationNote}) {
            clip.removeNote(note);
            note->setLocalStart(note->localStart() + 120);
            clip.insertNote(note);
        }
        const auto unchangedRecords =
            SingingClipPhonemeNormalizer::normalizeEditedOffsets(clip, previousWordStates);
        expect(unchangedRecords.isEmpty() && root->phonemeOffsetSeq().isEdited(),
               "moving the complete word preserves relative edited offsets");

        previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip);
        clip.removeNote(syllabificationNote);
        const auto membershipRecords =
            SingingClipPhonemeNormalizer::normalizeEditedOffsets(clip, previousWordStates);
        expect(membershipRecords.size() == 1 && membershipRecords.first().note == root &&
                   !root->phonemeOffsetSeq().isEdited(),
               "removing a syllabification note invalidates the word root offsets");
        SingingClipPhonemeNormalizer::restoreEditedOffsets(membershipRecords);
        clip.insertNote(syllabificationNote);

        previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip);
        auto changedPhonemes = root->phonemes();
        changedPhonemes.nameSeq.edited[3].isOnset = false;
        root->setPhonemes(changedPhonemes);
        const auto onsetChangeRecords =
            SingingClipPhonemeNormalizer::normalizeEditedOffsets(clip, previousWordStates);
        expect(onsetChangeRecords.size() == 1 && onsetChangeRecords.first().note == root &&
                   !root->phonemeOffsetSeq().isEdited(),
               "changing onset markers invalidates word root offsets");
    }

    void testTempoAwareWordState() {
        auto root = new Note;
        configureNote(*root, 0, "international+");
        Phonemes rootPhonemes;
        rootPhonemes.nameSeq.edited = internationalPhones();
        rootPhonemes.offsetSeq.edited = {0, 100, 200, 500, 900, 1000, 1500};
        root->setPhonemes(rootPhonemes);
        auto syllabificationNote = new Note;
        configureNote(*syllabificationNote, 480, "+");
        auto doubleSyllabificationNote = new Note;
        configureNote(*doubleSyllabificationNote, 960, "++");
        SingingClip clip({root, syllabificationNote, doubleSyllabificationNote});

        Timeline timeline;
        timeline.addTempo({960, 60.0});
        auto previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip, timeline);
        clip.setStart(720);
        const auto movedRecords = SingingClipPhonemeNormalizer::normalizeEditedOffsets(
            clip, previousWordStates, timeline);
        expect(movedRecords.size() == 1 && movedRecords.first().note == root &&
                   !root->phonemeOffsetSeq().isEdited(),
               "moving a complete word across a tempo boundary invalidates millisecond offsets");

        SingingClipPhonemeNormalizer::restoreEditedOffsets(movedRecords);
        previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(clip, timeline);
        timeline.addTempo({1200, 90.0});
        const auto tempoRecords = SingingClipPhonemeNormalizer::normalizeEditedOffsets(
            clip, previousWordStates, timeline);
        expect(tempoRecords.size() == 1 && tempoRecords.first().note == root &&
                   !root->phonemeOffsetSeq().isEdited(),
               "tempo changes inside a stationary word invalidate millisecond offsets");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testRanges();
    testStorageAndInferenceRoundTrip();
    testDetachedSyllabificationNotesStayOrphaned();
    testEditingEligibility();
    testRelativeTimingChangeInvalidatesEditedOffsets();
    testTempoAwareWordState();
    if (failures == 0)
        QTextStream(stdout) << "All Syllabification tests passed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
