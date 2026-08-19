#include "NoteActions.h"

#include "EditNotePositionAction.h"
#include "EditNoteStartAndLengthAction.h"
#include "EditNotesLengthAction.h"
#include "EditNoteWordPropertiesAction.h"
#include "EditPhonemeOffsetAction.h"
#include "InsertNoteAction.h"
#include "QuantizeNotesAction.h"
#include "RemoveNoteAction.h"
#include "SplitNoteAction.h"
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QCoreApplication>

#include <limits>

#include <utility>

namespace {

    HistoryFocus noteFocus(const QList<Note *> &notes, SingingClip *clip, const int deltaTick = 0,
                           const int deltaKey = 0, const int deltaLength = 0,
                           const bool resizeStart = false) {
        HistoryFocus focus;
        focus.kind = HistoryFocusKind::PianoRollNotes;
        focus.containerId = clip ? clip->id() : -1;
        focus.ticksAreLocal = true;
        focus.tickStart = std::numeric_limits<double>::max();
        focus.tickEnd = std::numeric_limits<double>::lowest();
        focus.valueStart = std::numeric_limits<double>::max();
        focus.valueEnd = std::numeric_limits<double>::lowest();
        for (const auto note : notes) {
            if (!note)
                continue;
            auto start = note->localStart() + deltaTick;
            auto length = note->length() + deltaLength;
            if (resizeStart)
                length = note->length() - deltaTick;
            focus.objectIds.append(note->id());
            focus.tickStart = qMin(focus.tickStart, static_cast<double>(start));
            focus.tickEnd = qMax(focus.tickEnd, static_cast<double>(start + length));
            focus.valueStart =
                qMin(focus.valueStart, static_cast<double>(note->keyIndex() + deltaKey));
            focus.valueEnd = qMax(focus.valueEnd, static_cast<double>(note->keyIndex() + deltaKey));
        }
        if (focus.objectIds.isEmpty())
            focus.tickStart = focus.tickEnd = focus.valueStart = focus.valueEnd = 0;
        return focus;
    }

} // namespace

void NoteActions::insertNotes(const QList<Note *> &notes, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Insert note(s)"));
    addAction(new InsertNoteAction(notes, clip));
    const auto focus = noteFocus(notes, clip);
    setFocusTransition({focus, focus});
}

void NoteActions::removeNotes(const QList<Note *> &notes, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Remove note(s)"));
    addAction(new RemoveNoteAction(notes, clip));
    const auto focus = noteFocus(notes, clip);
    setFocusTransition({focus, focus});
}

void NoteActions::editNotesStartAndLength(const QList<Note *> &notes, const int delta,
                                          SingingClip *clip) {
    setTranslatableName("NoteActions",
                        QT_TRANSLATE_NOOP("NoteActions", "Edit note start and length"));
    addAction(new EditNoteStartAndLengthAction(notes, delta, clip));
    setFocusTransition({noteFocus(notes, clip), noteFocus(notes, clip, delta, 0, 0, true)});
}

void NoteActions::editNotesLength(const QList<Note *> &notes, const int delta, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edit note length"));
    addAction(new EditNotesLengthAction(notes, delta, clip));
    setFocusTransition({noteFocus(notes, clip), noteFocus(notes, clip, 0, 0, delta)});
}

void NoteActions::editNotePosition(const QList<Note *> &notes, const int deltaTick,
                                   const int deltaKey, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edit note position"));
    addAction(new EditNotePositionAction(notes, deltaTick, deltaKey, clip));
    setFocusTransition({noteFocus(notes, clip), noteFocus(notes, clip, deltaTick, deltaKey)});
}

void NoteActions::editNotesWordProperties(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip,
                                          const QList<WordPropertyEditOptions> &options) {
    setTranslatableName("NoteActions",
                        QT_TRANSLATE_NOOP("NoteActions", "Edit note word properties"));
    addAction(new EditNoteWordPropertiesAction(notes, args, clip, options));
    const auto focus = noteFocus(notes, clip);
    setFocusTransition({focus, focus});
}

void NoteActions::editNotePhonemeOffset(Note *note, const QList<int> &offsets, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edite phoneme offset"));
    addAction(new EditPhonemeOffsetAction(note, offsets, clip));
    const auto focus = noteFocus({note}, clip);
    setFocusTransition({focus, focus});
}

void NoteActions::splitNote(Note *originalNote, Note *newNote, int newLength, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Split note"));
    addAction(new SplitNoteAction(originalNote, newNote, newLength, clip));
    const auto before = noteFocus({originalNote}, clip);
    auto after = noteFocus({originalNote}, clip);
    after.tickEnd = after.tickStart + newLength;
    const auto newNoteFocus = noteFocus({newNote}, clip);
    after.objectIds.append(newNoteFocus.objectIds);
    after.tickStart = qMin(after.tickStart, newNoteFocus.tickStart);
    after.tickEnd = qMax(after.tickEnd, newNoteFocus.tickEnd);
    after.valueStart = qMin(after.valueStart, newNoteFocus.valueStart);
    after.valueEnd = qMax(after.valueEnd, newNoteFocus.valueEnd);
    setFocusTransition({before, after});
}

void NoteActions::quantizeNotes(const QList<Note *> &notes,
                                const QList<QPair<int, int>> &newStartLengths,
                                SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Quantize notes"));
    QList<QuantizeNotesAction::Change> changes;
    changes.reserve(notes.size());
    for (int i = 0; i < notes.size(); ++i) {
        const auto note = notes.at(i);
        if (!note)
            continue;
        QuantizeNotesAction::Change change;
        change.note = note;
        change.oldStart = note->localStart();
        change.oldLength = note->length();
        change.newStart = newStartLengths.at(i).first;
        change.newLength = newStartLengths.at(i).second;
        changes.append(change);
    }
    addAction(new QuantizeNotesAction(std::move(changes), clip));

    const auto before = noteFocus(notes, clip);
    auto after = before;
    after.tickStart = std::numeric_limits<double>::max();
    after.tickEnd = std::numeric_limits<double>::lowest();
    for (const auto &change : changes) {
        after.tickStart = qMin(after.tickStart, static_cast<double>(change.newStart));
        after.tickEnd =
            qMax(after.tickEnd, static_cast<double>(change.newStart + change.newLength));
    }
    if (changes.isEmpty())
        after.tickStart = after.tickEnd = 0;
    setFocusTransition({before, after});
}
