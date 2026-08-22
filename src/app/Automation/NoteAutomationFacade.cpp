#include "NoteAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "Controller/Actions/AppModel/Note/EditNoteWordPropertiesAction.h"

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/NoteResizeUtils.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QSet>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace Automation {
    namespace {
        QByteArray noteIdsFingerprint(const ClipId clipId, const QList<NoteId> &noteIds,
                                      const QList<int> &extra = {}) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            QList<int> ids;
            ids.reserve(noteIds.size());
            for (const auto id : noteIds)
                ids.append(id.value());
            stream << clipId.value() << ids << extra;
            return result;
        }

        void hashString(QCryptographicHash &hash, const QString &value) {
            const auto bytes = value.toUtf8();
            hash.addData(QByteArray::number(bytes.size()));
            hash.addData(":", 1);
            hash.addData(bytes);
        }

        void hashInteger(QCryptographicHash &hash, const qint64 value) {
            hash.addData(QByteArray::number(value));
            hash.addData(";", 1);
        }

        void hashNoteDraft(QCryptographicHash &hash, const NoteDraftDto &note) {
            hashString(hash, note.clientRef);
            hashInteger(hash, note.localStart);
            hashInteger(hash, note.length);
            hashInteger(hash, note.keyIndex);
            hashInteger(hash, note.centShift);
            hashString(hash, note.lyric);
            hashString(hash, note.language);
            hashString(hash, note.pronunciation.original);
            hashString(hash, note.pronunciation.edited);
            hashInteger(hash, note.pronunciationCandidates.size());
            for (const auto &candidate : note.pronunciationCandidates)
                hashString(hash, candidate);
            hash.addData(QJsonDocument(note.phonemes.serialize()).toJson(QJsonDocument::Compact));
            hashInteger(hash, note.lineFeed);
            hashInteger(hash, note.workspace.size());
            for (auto it = note.workspace.cbegin(); it != note.workspace.cend(); ++it) {
                hashString(hash, it.key());
                hash.addData(QJsonDocument(it.value()).toJson(QJsonDocument::Compact));
            }
        }

        QByteArray insertFingerprint(const ClipId clipId, const QList<NoteDraftDto> &notes) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, clipId.value());
            hashInteger(hash, notes.size());
            for (const auto &note : notes)
                hashNoteDraft(hash, note);
            return hash.result();
        }

        QByteArray splitFingerprint(const ClipId clipId, const NoteId noteId,
                                    const NoteDraftDto &newNote, const int newLength) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, clipId.value());
            hashInteger(hash, noteId.value());
            hashInteger(hash, newLength);
            hashNoteDraft(hash, newNote);
            return hash.result();
        }

        QByteArray wordEditsFingerprint(const ClipId clipId, const QList<NoteWordEditDto> &edits) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, clipId.value());
            hashInteger(hash, edits.size());
            for (const auto &edit : edits) {
                hashInteger(hash, edit.noteId.value());
                hashString(hash, edit.lyric);
                hashString(hash, edit.language);
                hashString(hash, edit.pronunciation.original);
                hashString(hash, edit.pronunciation.edited);
                hashInteger(hash, edit.pronunciationCandidates.size());
                for (const auto &candidate : edit.pronunciationCandidates)
                    hashString(hash, candidate);
                hash.addData(
                    QJsonDocument(edit.phonemes.serialize()).toJson(QJsonDocument::Compact));
                hashInteger(hash, edit.replacePronunciation);
                hashInteger(hash, edit.replacePronunciationCandidates);
            }
            return hash.result();
        }

        bool validNoteDraft(const NoteDraftDto &note) {
            return note.localStart >= 0 && note.length > 0 && note.keyIndex >= 0 &&
                   note.keyIndex <= 127;
        }

        bool wordPropertiesEqual(const Note::WordProperties &left,
                                 const Note::WordProperties &right) {
            return left.lyric == right.lyric && left.language == right.language &&
                   left.pronunciation.original == right.pronunciation.original &&
                   left.pronunciation.edited == right.pronunciation.edited &&
                   left.pronCandidates == right.pronCandidates &&
                   left.phonemes.serialize() == right.phonemes.serialize();
        }

        Note::WordProperties normalizedWordProperties(const Note::WordProperties &previous,
                                                      const NoteWordEditDto &edit) {
            Note::WordProperties next;
            next.lyric = edit.lyric.trimmed();
            next.language = edit.language;
            next.pronunciation = edit.pronunciation;
            next.pronCandidates = edit.pronunciationCandidates;
            next.phonemes = edit.phonemes;
            const bool wordInputChanged =
                previous.lyric != next.lyric || previous.language != next.language;
            if (wordInputChanged) {
                if (!edit.replacePronunciation)
                    next.pronunciation.edited.clear();
                if (!edit.replacePronunciationCandidates)
                    next.pronCandidates.clear();
            }
            auto appliedPronunciation = previous.pronunciation;
            appliedPronunciation.edited = next.pronunciation.edited;
            if (wordInputChanged ||
                previous.pronunciation.result() != appliedPronunciation.result()) {
                next.phonemes = {};
            }
            return next;
        }

        QList<ObjectRef> noteRefs(const QList<NoteId> &ids) {
            QList<ObjectRef> result;
            result.reserve(ids.size());
            for (const auto id : ids)
                result.append({ObjectKind::Note, id.value()});
            return result;
        }

        bool hasDuplicateIds(const QList<NoteId> &ids) {
            QSet<int> seen;
            for (const auto id : ids) {
                if (seen.contains(id.value()))
                    return true;
                seen.insert(id.value());
            }
            return false;
        }
    }

    NoteAutomationFacade::NoteAutomationFacade(OperationCatalog &catalog,
                                               AutomationDispatcher &dispatcher,
                                               CommandCommitter &committer,
                                               DocumentObjectResolver &objects)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer), m_objects(objects) {
        registerOperations();
    }

    AutomationResult<QList<NoteSnapshotDto>>
        NoteAutomationFacade::getNotes(const DocumentId &documentId, const ClipId clipId) {
        return m_dispatcher.dispatchDocumentQuery<QList<NoteSnapshotDto>>(
            OperationIds::notes::get, documentId, [this, clipId](DocumentSession &session) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<QList<NoteSnapshotDto>>(resolved.getError());
                const auto *clip = static_cast<const SingingClip *>(resolved.get().clip);
                QList<NoteSnapshotDto> result;
                result.reserve(clip->notes().count());
                for (const auto *note : clip->notes())
                    result.append({NoteId(note->id()), clipId, noteDraftDto(*note)});
                return AutomationResult<QList<NoteSnapshotDto>>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::insertNotes(const CommandContext &context, const ClipId clipId,
                                          const QList<NoteDraftDto> &notes) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::insert, context, insertFingerprint(clipId, notes),
            [this, clipId, notes](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                for (const auto &note : notes) {
                    if (!validNoteDraft(note)) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("notes"),
                            QStringLiteral("Note geometry or key is invalid")));
                    }
                }
                auto clientRefValidation = validateClientRefs(notes);
                if (!clientRefValidation)
                    return AutomationResult<MutationResult>(clientRefValidation.getError());
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !notes.isEmpty()));
                if (notes.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                std::vector<std::unique_ptr<Note>> ownedNotes;
                ownedNotes.reserve(static_cast<size_t>(notes.size()));
                QList<Note *> rawNotes;
                QList<ObjectRef> affected;
                QList<CreatedObjectRef> createdObjects;
                for (const auto &draft : notes) {
                    auto note = buildNote(draft, clip, &createdObjects);
                    rawNotes.append(note.get());
                    affected.append({ObjectKind::Note, note->id()});
                    ownedNotes.push_back(std::move(note));
                }
                auto actions = std::make_unique<NoteActions>();
                actions->insertNotes(rawNotes, clip, resolved.get().track);
                auto result = m_committer.commit(session, std::move(actions), affected,
                                                 std::move(createdObjects));
                if (result) {
                    for (auto &note : ownedNotes)
                        note.release();
                }
                return result;
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::removeNotes(const CommandContext &context, const ClipId clipId,
                                          QList<NoteId> noteIds) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::remove, context, noteIdsFingerprint(clipId, noteIds),
            [this, clipId, noteIds = std::move(noteIds), duplicates](DocumentSession &session,
                                                                     const bool validateOnly) {
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                QList<Note *> notes;
                for (const auto id : noteIds) {
                    auto resolved = m_objects.note(session, clipId, id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    notes.append(resolved.get().note);
                }
                if (duplicates) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("note_ids"), QStringLiteral("Note IDs must be unique")));
                }
                const auto affected = noteRefs(noteIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !notes.isEmpty(), affected));
                if (notes.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->removeNotes(notes, static_cast<SingingClip *>(clipResult.get().clip));
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> NoteAutomationFacade::moveNotes(const CommandContext &context,
                                                                     const ClipId clipId,
                                                                     QList<NoteId> noteIds,
                                                                     const int deltaTick,
                                                                     const int deltaKey) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::move, context,
            noteIdsFingerprint(clipId, noteIds, {deltaTick, deltaKey}),
            [this, clipId, noteIds = std::move(noteIds), deltaTick, duplicates,
             deltaKey](DocumentSession &session, const bool validateOnly) {
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                QList<Note *> notes;
                int minimumStart = std::numeric_limits<int>::max();
                for (const auto id : noteIds) {
                    auto note = m_objects.note(session, clipId, id);
                    if (!note)
                        return AutomationResult<MutationResult>(note.getError());
                    notes.append(note.get().note);
                    minimumStart = std::min(minimumStart, note.get().note->localStart());
                }
                if (duplicates) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("note_ids"), QStringLiteral("Note IDs must be unique")));
                }
                const auto safeDeltaTick =
                    notes.isEmpty() ? 0
                                    : NoteResizeUtils::clampLeftMoveDelta(deltaTick, minimumStart);
                for (const auto *note : notes) {
                    const auto targetKey = note->keyIndex() + deltaKey;
                    if (targetKey < 0 || targetKey > 127) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("delta_key"),
                            QStringLiteral("Moved note key is out of range")));
                    }
                }
                const bool changed = !notes.isEmpty() && (safeDeltaTick != 0 || deltaKey != 0);
                const auto affected = noteRefs(noteIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotePosition(notes, safeDeltaTick, deltaKey,
                                          static_cast<SingingClip *>(clipResult.get().clip),
                                          clipResult.get().track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::resizeNotesLeft(const CommandContext &context, const ClipId clipId,
                                              QList<NoteId> noteIds, const int deltaTick,
                                              const int minimumLength) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::resize_left, context,
            noteIdsFingerprint(clipId, noteIds, {deltaTick, minimumLength}),
            [this, clipId, noteIds = std::move(noteIds), deltaTick, duplicates,
             minimumLength](DocumentSession &session, const bool validateOnly) {
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                QList<Note *> notes;
                int minimumStart = std::numeric_limits<int>::max();
                for (const auto id : noteIds) {
                    auto note = m_objects.note(session, clipId, id);
                    if (!note)
                        return AutomationResult<MutationResult>(note.getError());
                    notes.append(note.get().note);
                    minimumStart = std::min(minimumStart, note.get().note->localStart());
                }
                if (minimumLength <= 0 || duplicates) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("resize"), QStringLiteral("Resize input is invalid")));
                }
                int safeDelta = deltaTick;
                for (const auto *note : notes)
                    safeDelta =
                        NoteResizeUtils::clampLeftDelta(note->length(), safeDelta, minimumLength);
                if (!notes.isEmpty())
                    safeDelta = NoteResizeUtils::clampLeftMoveDelta(safeDelta, minimumStart);
                const bool changed = !notes.isEmpty() && safeDelta != 0;
                const auto affected = noteRefs(noteIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotesStartAndLength(notes, safeDelta,
                                                 static_cast<SingingClip *>(clipResult.get().clip),
                                                 clipResult.get().track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::resizeNotesRight(const CommandContext &context, const ClipId clipId,
                                               QList<NoteId> noteIds, const int deltaTick,
                                               const int minimumLength) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::resize_right, context,
            noteIdsFingerprint(clipId, noteIds, {deltaTick, minimumLength}),
            [this, clipId, noteIds = std::move(noteIds), deltaTick, duplicates,
             minimumLength](DocumentSession &session, const bool validateOnly) {
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                QList<Note *> notes;
                for (const auto id : noteIds) {
                    auto note = m_objects.note(session, clipId, id);
                    if (!note)
                        return AutomationResult<MutationResult>(note.getError());
                    notes.append(note.get().note);
                }
                if (minimumLength <= 0 || duplicates) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("resize"), QStringLiteral("Resize input is invalid")));
                }
                int safeDelta = deltaTick;
                for (const auto *note : notes)
                    safeDelta =
                        NoteResizeUtils::clampRightDelta(note->length(), safeDelta, minimumLength);
                const bool changed = !notes.isEmpty() && safeDelta != 0;
                const auto affected = noteRefs(noteIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotesLength(notes, safeDelta,
                                         static_cast<SingingClip *>(clipResult.get().clip),
                                         clipResult.get().track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> NoteAutomationFacade::splitNote(const CommandContext &context,
                                                                     const ClipId clipId,
                                                                     const NoteId noteId,
                                                                     const NoteDraftDto &newNote,
                                                                     const int newLength) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::split, context,
            splitFingerprint(clipId, noteId, newNote, newLength),
            [this, clipId, noteId, newNote, newLength](DocumentSession &session,
                                                       const bool validateOnly) {
                auto resolved = m_objects.note(session, clipId, noteId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!validNoteDraft(newNote) || newLength <= 0 ||
                    newLength >= resolved.get().note->length()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("split"), QStringLiteral("Split note geometry is invalid")));
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, true));
                QList<CreatedObjectRef> createdObjects;
                auto note = buildNote(newNote, resolved.get().clip, &createdObjects);
                const auto createdId = note->id();
                auto actions = std::make_unique<NoteActions>();
                actions->splitNote(resolved.get().note, note.get(), newLength, resolved.get().clip);
                auto result = m_committer.commit(
                    session, std::move(actions),
                    {
                        {ObjectKind::Note, noteId.value()},
                        {ObjectKind::Note, createdId     }
                },
                    std::move(createdObjects));
                if (result)
                    note.release();
                return result;
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setPhonemeOffsets(const CommandContext &context, const ClipId clipId,
                                                const NoteId noteId, const QList<int> &offsets) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::set_phoneme_offsets, context,
            noteIdsFingerprint(clipId, {noteId}, offsets),
            [this, clipId, noteId, offsets](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.note(session, clipId, noteId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                const auto phonemeCount = resolved.get().note->phonemeNameSeq().result().count();
                if (!offsets.isEmpty() &&
                    (offsets.count() != phonemeCount ||
                     !std::is_sorted(offsets.cbegin(), offsets.cend()))) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("offsets"),
                        QStringLiteral("Phoneme offsets must match the phoneme count and be sorted")));
                }
                const bool changed = resolved.get().note->phonemeOffsetSeq().edited != offsets;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Note, noteId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotePhonemeOffset(resolved.get().note, offsets, resolved.get().clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::quantizeNotes(const CommandContext &context, const ClipId clipId,
                                            QList<NoteId> noteIds, const int quantize,
                                            const bool quantizeStart, const bool quantizeLength) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::quantize, context,
            noteIdsFingerprint(clipId, noteIds, {quantize, quantizeStart, quantizeLength}),
            [this, clipId, noteIds = std::move(noteIds), quantize, quantizeStart, duplicates,
             quantizeLength](DocumentSession &session, const bool validateOnly) {
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                auto *clip = static_cast<SingingClip *>(clipResult.get().clip);
                QList<Note *> notes;
                for (const auto id : noteIds) {
                    auto resolved = m_objects.note(session, clipId, id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    notes.append(resolved.get().note);
                }
                if (duplicates) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("note_ids"), QStringLiteral("Note IDs must be unique")));
                }
                const auto grid = TimelineSnapUtils::quantizeToTicks(quantize);
                if (grid <= 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("quantize"), QStringLiteral("Quantize value is invalid")));
                }
                QList<QPair<int, int>> geometry;
                bool changed = false;
                for (auto *note : notes) {
                    auto start = note->localStart();
                    auto length = note->length();
                    if (quantizeStart) {
                        const auto snapped = TimelineSnapUtils::snapNearest(
                            clip->start() + start, grid, session.model()->timeline());
                        start = qMax(0, snapped - clip->start());
                    }
                    if (quantizeLength)
                        length = qMax(grid, TimelineSnapUtils::snapNearest(length, grid));
                    geometry.append({start, length});
                    changed |= start != note->localStart() || length != note->length();
                }
                const auto affected = noteRefs(noteIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->quantizeNotes(notes, geometry, clip, clipResult.get().track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setWordProperties(const CommandContext &context, const ClipId clipId,
                                                QList<NoteWordEditDto> edits) {
        std::sort(edits.begin(), edits.end(),
                  [](const NoteWordEditDto &left, const NoteWordEditDto &right) {
                      return left.noteId.value() < right.noteId.value();
                  });
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::set_word_properties, context,
            wordEditsFingerprint(clipId, edits),
            [this, clipId, edits = std::move(edits)](DocumentSession &session,
                                                     const bool validateOnly) {
                QList<NoteId> ids;
                ids.reserve(edits.size());
                for (const auto &edit : edits)
                    ids.append(edit.noteId);
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());
                QList<Note *> resolvedNotes;
                resolvedNotes.reserve(edits.size());
                for (const auto &edit : edits) {
                    auto resolved = m_objects.note(session, clipId, edit.noteId);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    resolvedNotes.append(resolved.get().note);
                }
                if (hasDuplicateIds(ids)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("edits"), QStringLiteral("Note IDs must be unique")));
                }
                QList<Note *> changedNotes;
                QList<Note::WordProperties> properties;
                QList<WordPropertyEditOptions> options;
                QList<NoteId> changedIds;
                for (int index = 0; index < edits.size(); ++index) {
                    const auto &edit = edits.at(index);
                    auto *note = resolvedNotes.at(index);
                    const auto previous = Note::WordProperties::fromNote(*note);
                    const auto normalized = normalizedWordProperties(previous, edit);
                    if (wordPropertiesEqual(previous, normalized))
                        continue;
                    changedNotes.append(note);
                    properties.append(normalized);
                    options.append(
                        {edit.replacePronunciation, edit.replacePronunciationCandidates});
                    changedIds.append(edit.noteId);
                }
                const auto affected = noteRefs(changedIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !changedNotes.isEmpty(), affected));
                if (changedNotes.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotesWordProperties(changedNotes, properties,
                                                 static_cast<SingingClip *>(clipResult.get().clip),
                                                 options);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    void NoteAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = OperationIds::notes::get,
            .category = QStringLiteral("notes"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addMutation = [&add](const OperationId &id) {
            add({
                .id = id,
                .category = QStringLiteral("notes"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Write,
                .revisionPolicy = RevisionPolicy::Increment,
                .historyPolicy = HistoryPolicy::Record,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::DocumentGeneration,
            });
        };
        addMutation(OperationIds::notes::insert);
        addMutation(OperationIds::notes::move);
        addMutation(OperationIds::notes::quantize);
        addMutation(OperationIds::notes::remove);
        addMutation(OperationIds::notes::resize_left);
        addMutation(OperationIds::notes::resize_right);
        addMutation(OperationIds::notes::set_phoneme_offsets);
        addMutation(OperationIds::notes::set_word_properties);
        addMutation(OperationIds::notes::split);
    }

} // namespace Automation
