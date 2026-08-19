#include "Controller/Actions/AppModel/Note/EditNoteWordPropertiesAction.h"

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

    PhonemeName phone(const char *name, const bool onset = false) {
        PhonemeName result;
        result.language = QStringLiteral("eng");
        result.name = QString::fromLatin1(name);
        result.isOnset = onset;
        return result;
    }

    Phonemes phonemes(const char *originalFirst, const char *originalSecond,
                      const char *editedFirst, const char *editedSecond) {
        Phonemes result;
        result.nameSeq.original = {phone(originalFirst, true), phone(originalSecond)};
        result.nameSeq.edited = {phone(editedFirst, true), phone(editedSecond)};
        result.offsetSeq.original = {0, 120};
        return result;
    }

    bool phonemesEqual(const Phonemes &left, const Phonemes &right) {
        return left.nameSeq.original == right.nameSeq.original &&
               left.nameSeq.edited == right.nameSeq.edited &&
               left.offsetSeq.original == right.offsetSeq.original &&
               left.offsetSeq.edited == right.offsetSeq.edited;
    }

    bool phonemesEmpty(const Phonemes &value) {
        return value.nameSeq.original.isEmpty() && value.nameSeq.edited.isEmpty() &&
               value.offsetSeq.original.isEmpty() && value.offsetSeq.edited.isEmpty();
    }

    Note *makeNote(const QString &lyric, const QString &editedPronunciation,
                   const Phonemes &phonemeData) {
        auto *note = new Note;
        note->setLyric(lyric);
        note->setLanguage(QStringLiteral("eng"));
        note->setPronunciation(Pronunciation(lyric.toLower(), editedPronunciation));
        note->setPronCandidates({lyric.toLower(), lyric.toUpper()});
        note->setPhonemes(phonemeData);
        return note;
    }

    void testLyricChangeResetsPronunciationAndPhonemes() {
        const auto oldPhonemes = phonemes("l", "a", "k", "a");
        auto *note = makeNote(QStringLiteral("la"), QStringLiteral("custom"), oldPhonemes);
        const auto oldCandidates = note->pronCandidates();
        SingingClip clip({note});

        auto next = Note::WordProperties::fromNote(*note);
        next.lyric = QStringLiteral("mi");
        EditNoteWordPropertiesAction action({note}, {next}, &clip);
        action.execute();

        expect(note->lyric() == QStringLiteral("mi"), "lyric edit must be applied");
        expect(note->pronunciation().edited.isEmpty(),
               "lyric edit must clear the previous edited pronunciation");
        expect(note->pronCandidates().isEmpty(),
               "lyric edit must clear pronunciation candidates derived from the old lyric");
        expect(phonemesEmpty(note->phonemes()),
               "lyric edit must clear phonemes derived from the old pronunciation");

        action.undo();
        expect(note->lyric() == QStringLiteral("la") &&
                   note->pronunciation().edited == QStringLiteral("custom") &&
                   note->pronCandidates() == oldCandidates &&
                   phonemesEqual(note->phonemes(), oldPhonemes),
               "undo must restore every word property");

        action.execute();
        expect(note->pronunciation().edited.isEmpty() && phonemesEmpty(note->phonemes()),
               "redo must apply the same cascade reset");
    }

    void testPronunciationChangeResetsPhonemes() {
        const auto oldPhonemes = phonemes("m", "i", "n", "i");
        auto *note = makeNote(QStringLiteral("mi"), QStringLiteral("mee"), oldPhonemes);
        SingingClip clip({note});

        auto next = Note::WordProperties::fromNote(*note);
        next.pronunciation.edited = QStringLiteral("ma");
        EditNoteWordPropertiesAction action({note}, {next}, &clip,
                                            {.replacePronunciation = true});
        action.execute();

        expect(note->pronunciation().edited == QStringLiteral("ma"),
               "pronunciation edit must be applied");
        expect(phonemesEmpty(note->phonemes()),
               "pronunciation edit must clear phonemes derived from the old pronunciation");

        action.undo();
        expect(note->pronunciation().edited == QStringLiteral("mee") &&
                   phonemesEqual(note->phonemes(), oldPhonemes),
               "undo must restore pronunciation and phonemes");
    }

    void testUnchangedUpperPropertiesPreservePhonemes() {
        const auto oldPhonemes = phonemes("m", "i", "n", "i");
        auto *note = makeNote(QStringLiteral("mi"), QStringLiteral("mee"), oldPhonemes);
        SingingClip clip({note});

        auto next = Note::WordProperties::fromNote(*note);
        next.lyric = QStringLiteral("  mi  ");
        next.pronCandidates = {QStringLiteral("mi"), QStringLiteral("mee")};
        EditNoteWordPropertiesAction action({note}, {next}, &clip);
        action.execute();

        expect(note->lyric() == QStringLiteral("mi"), "lyric comparison must use stored form");
        expect(note->pronunciation().edited == QStringLiteral("mee"),
               "unchanged lyric must preserve the edited pronunciation");
        expect(phonemesEqual(note->phonemes(), oldPhonemes),
               "unchanged lyric and pronunciation must preserve edited phonemes");
    }

    void testEquivalentPronunciationPreservesPhonemes() {
        const auto oldPhonemes = phonemes("m", "i", "n", "i");
        auto *note = makeNote(QStringLiteral("mi"), {}, oldPhonemes);
        SingingClip clip({note});

        auto next = Note::WordProperties::fromNote(*note);
        next.pronunciation.edited = QStringLiteral("mi");
        EditNoteWordPropertiesAction action({note}, {next}, &clip,
                                            {.replacePronunciation = true});
        action.execute();

        expect(note->pronunciation().edited == QStringLiteral("mi"),
               "equivalent pronunciation representation must be applied");
        expect(phonemesEqual(note->phonemes(), oldPhonemes),
               "an unchanged effective pronunciation must preserve edited phonemes");
    }

    void testEqualFillLyricReplacementsArePreserved() {
        const auto oldPhonemes = phonemes("l", "a", "k", "a");
        auto *note = makeNote(QStringLiteral("la"), QStringLiteral("custom"), oldPhonemes);
        note->setPronCandidates({QStringLiteral("shared")});
        SingingClip clip({note});

        auto next = Note::WordProperties::fromNote(*note);
        next.lyric = QStringLiteral("mi");
        EditNoteWordPropertiesAction action(
            {note}, {next}, &clip,
            {.replacePronunciation = true, .replacePronCandidates = true});
        action.execute();

        expect(note->pronunciation().edited == QStringLiteral("custom") &&
                   note->pronCandidates() == next.pronCandidates,
               "fill lyric must preserve explicitly supplied value-equal replacements");
        expect(phonemesEmpty(note->phonemes()),
               "fill lyric must still reset phonemes after changing the lyric");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testLyricChangeResetsPronunciationAndPhonemes();
    testPronunciationChangeResetsPhonemes();
    testUnchangedUpperPropertiesPreservePhonemes();
    testEquivalentPronunciationPreservesPhonemes();
    testEqualFillLyricReplacementsArePreserved();
    if (failures == 0)
        QTextStream(stdout) << "All word property cascade tests passed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
