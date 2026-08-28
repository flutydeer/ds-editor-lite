#include "NoteAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "Controller/Actions/AppModel/Note/EditNoteWordPropertiesAction.h"
#include "Controller/Actions/AppModel/Param/ReplaceParamAction.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/NoteResizeUtils.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QRegularExpression>
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

        void hashCurveDraft(QCryptographicHash &hash, const CurveDraftDto &curve) {
            hashInteger(hash, static_cast<int>(curve.type));
            hashInteger(hash, curve.localStart);
            hashInteger(hash, curve.step);
            hashInteger(hash, curve.values.size());
            for (const auto value : curve.values)
                hashInteger(hash, value);
            hashInteger(hash, curve.nodes.size());
            for (const auto &node : curve.nodes) {
                hashInteger(hash, node.position);
                hashInteger(hash, node.value);
                hashInteger(hash, static_cast<int>(node.interpolation));
            }
        }

        QByteArray transferFingerprint(const ClipId targetClipId, const int targetStart,
                                       const NoteTransferPayload &payload) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, targetClipId.value());
            hashInteger(hash, targetStart);
            hashInteger(hash, payload.sourceStart);
            hashInteger(hash, payload.sourceEnd);
            hashInteger(hash, payload.notes.size());
            for (const auto &note : payload.notes)
                hashNoteDraft(hash, note);
            hashInteger(hash, payload.parameters.size());
            for (const auto &parameter : payload.parameters) {
                hashInteger(hash, parameter.name);
                hashInteger(hash, parameter.type);
                hashInteger(hash, parameter.curves.size());
                for (const auto &curve : parameter.curves)
                    hashCurveDraft(hash, curve);
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

        class SetOriginalPronunciationAction final : public IAction {
        public:
            SetOriginalPronunciationAction(Note *note, SingingClip *clip, QString pronunciation)
                : m_note(note), m_clip(clip), m_oldPronunciation(note->pronunciation()),
                  m_oldPhonemes(note->phonemes()), m_newPronunciation(std::move(pronunciation)) {
                auto next = m_oldPronunciation;
                next.original = m_newPronunciation;
                m_resetPhonemes = next.result() != m_oldPronunciation.result();
            }

            void execute() override {
                m_note->setPronunciation(Note::Original, m_newPronunciation);
                if (m_resetPhonemes)
                    m_note->setPhonemes(Phonemes{});
                notify();
            }

            void undo() override {
                m_note->setPronunciation(Note::Original, m_oldPronunciation.original);
                if (m_resetPhonemes)
                    m_note->setPhonemes(m_oldPhonemes);
                notify();
            }

        private:
            void notify() const {
                m_clip->notifyNoteChanged(SingingClip::EditedPronunciationOnly, {m_note});
            }

            Note *m_note;
            SingingClip *m_clip;
            Pronunciation m_oldPronunciation;
            Phonemes m_oldPhonemes;
            QString m_newPronunciation;
            bool m_resetPhonemes = false;
        };

        class SetOriginalPronunciationActions final : public ActionSequence {
        public:
            SetOriginalPronunciationActions(Note *note, SingingClip *clip, QString pronunciation) {
                addAction(new SetOriginalPronunciationAction(note, clip, std::move(pronunciation)));
            }
        };

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

        bool validCurveDraft(const ParamInfo::Name name, const CurveDraftDto &curve,
                             const int sourceStart, const int sourceEnd) {
            const auto spec = ParamInfo::valueSpec(name);
            const auto validValue = [&spec](const int value) {
                return value >= spec.minimum && value <= spec.maximum &&
                       (value - spec.minimum) % spec.step == 0;
            };
            if (curve.type == CurveDraftDto::Type::Draw) {
                const auto end = static_cast<qint64>(curve.localStart) +
                                 static_cast<qint64>(curve.step) * curve.values.size();
                return curve.step > 0 && !curve.values.isEmpty() &&
                       curve.localStart >= sourceStart && end <= sourceEnd &&
                       std::all_of(curve.values.cbegin(), curve.values.cend(), validValue);
            }
            if (curve.nodes.size() < 2)
                return false;
            auto previous = std::numeric_limits<int>::min();
            for (const auto &node : curve.nodes) {
                if (node.position < sourceStart || node.position > sourceEnd ||
                    node.position <= previous || !validValue(node.value)) {
                    return false;
                }
                previous = node.position;
            }
            return true;
        }

        bool validTransferPayload(const NoteTransferPayload &payload) {
            if (payload.notes.isEmpty() || payload.sourceStart < 0 ||
                payload.sourceEnd <= payload.sourceStart) {
                return false;
            }
            for (const auto &note : payload.notes) {
                const auto end = static_cast<qint64>(note.localStart) + note.length;
                if (!validNoteDraft(note) || note.localStart < payload.sourceStart ||
                    end > payload.sourceEnd) {
                    return false;
                }
            }
            QSet<QPair<int, int>> seenParameters;
            for (const auto &parameter : payload.parameters) {
                if (parameter.name < ParamInfo::Pitch || parameter.name > ParamInfo::ToneShift ||
                    (parameter.type != Param::Edited && parameter.type != Param::Envelope)) {
                    return false;
                }
                const auto key =
                    qMakePair(static_cast<int>(parameter.name), static_cast<int>(parameter.type));
                if (seenParameters.contains(key) || parameter.curves.isEmpty())
                    return false;
                seenParameters.insert(key);
                for (const auto &curve : parameter.curves) {
                    if (!validCurveDraft(parameter.name, curve, payload.sourceStart,
                                         payload.sourceEnd)) {
                        return false;
                    }
                }
            }
            return true;
        }

        class NoteTransferActions final : public NoteActions {
        public:
            void replaceParameter(const ParamInfo::Name name, const Param::Type type,
                                  const QList<Curve *> &curves, SingingClip *clip) {
                addAction(new ReplaceParamAction(name, type, curves, clip));
            }
        };
    }

    NoteAutomationFacade::NoteAutomationFacade(AutomationDispatcher &dispatcher,
                                               CommandCommitter &committer,
                                               DocumentObjectResolver &objects)
        : m_dispatcher(dispatcher), m_committer(committer), m_objects(objects) {
    }

    AutomationResult<QList<NoteSnapshotDto>>
        NoteAutomationFacade::getNotes(const DocumentId &documentId, const ClipId clipId) {
        return m_dispatcher.dispatchDocumentQuery<QList<NoteSnapshotDto>>(
            OperationIds::notes::list, documentId, [this, clipId](DocumentSession &session) {
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

    AutomationResult<QList<NoteSearchMatchDto>>
        NoteAutomationFacade::searchNotes(const DocumentId &documentId, const ClipId clipId,
                                          const QString &query, const QString &mode,
                                          const bool caseSensitive, const bool regularExpression) {
        return m_dispatcher.dispatchDocumentQuery<QList<NoteSearchMatchDto>>(
            OperationIds::notes::search, documentId,
            [this, clipId, query, mode, caseSensitive,
             regularExpression](DocumentSession &session) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<QList<NoteSearchMatchDto>>(resolved.getError());
                if (mode != QStringLiteral("starts_with") && mode != QStringLiteral("exact") &&
                    mode != QStringLiteral("contains")) {
                    return AutomationResult<QList<NoteSearchMatchDto>>(
                        AutomationError::invalidArgument(QStringLiteral("mode"),
                                                         QStringLiteral("Search mode is invalid")));
                }
                const auto sensitivity = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                QRegularExpression expression;
                if (regularExpression) {
                    QRegularExpression::PatternOptions options =
                        QRegularExpression::NoPatternOption;
                    if (!caseSensitive)
                        options |= QRegularExpression::CaseInsensitiveOption;
                    expression.setPatternOptions(options);
                    expression.setPattern(query);
                    if (!expression.isValid()) {
                        return AutomationResult<QList<NoteSearchMatchDto>>(
                            AutomationError::invalidArgument(
                                QStringLiteral("query"),
                                QStringLiteral("Regular expression is invalid")));
                    }
                }
                QList<NoteSearchMatchDto> result;
                const auto *clip = static_cast<const SingingClip *>(resolved.get().clip);
                for (const auto *note : clip->notes()) {
                    bool matches = false;
                    if (regularExpression) {
                        const auto match = expression.match(note->lyric());
                        matches =
                            match.hasMatch() &&
                            (mode != QStringLiteral("exact") ||
                             match.capturedLength() == note->lyric().size()) &&
                            (mode != QStringLiteral("starts_with") || match.capturedStart() == 0);
                    } else if (mode == QStringLiteral("exact")) {
                        matches = note->lyric().compare(query, sensitivity) == 0;
                    } else if (mode == QStringLiteral("starts_with")) {
                        matches = note->lyric().startsWith(query, sensitivity);
                    } else {
                        matches = note->lyric().contains(query, sensitivity);
                    }
                    if (matches) {
                        result.append({NoteId(note->id()), note->localStart(), note->length(),
                                       note->lyric()});
                    }
                }
                return AutomationResult<QList<NoteSearchMatchDto>>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::insertNotes(const CommandContext &context, const ClipId clipId,
                                          const QList<NoteDraftDto> &notes) {
        const auto requestFingerprint =
            context.idempotencyKey.isEmpty() ? QByteArray{} : insertFingerprint(clipId, notes);
        return m_dispatcher.dispatchIdempotentDocumentCommand(
            OperationIds::notes::insert, context, requestFingerprint,
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
        NoteAutomationFacade::duplicateNotes(const CommandContext &context,
                                             const ClipId sourceClipId, QList<NoteId> noteIds,
                                             const ClipId targetClipId, const int targetStart) {
        const auto requestFingerprint =
            context.idempotencyKey.isEmpty()
                ? QByteArray{}
                : noteIdsFingerprint(sourceClipId, noteIds, {targetClipId.value(), targetStart});
        return m_dispatcher.dispatchIdempotentDocumentCommand(
            OperationIds::notes::duplicate, context, requestFingerprint,
            [this, sourceClipId, noteIds = std::move(noteIds), targetClipId,
             targetStart](DocumentSession &session, const bool validateOnly) {
                auto source = m_objects.singingClip(session, sourceClipId);
                if (!source)
                    return AutomationResult<MutationResult>(source.getError());
                auto target = m_objects.singingClip(session, targetClipId);
                if (!target)
                    return AutomationResult<MutationResult>(target.getError());
                if (targetStart < 0 || hasDuplicateIds(noteIds)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target_start"),
                        QStringLiteral("Duplicate note input is invalid")));
                }
                QList<Note *> sourceNotes;
                for (const auto id : noteIds) {
                    auto note = m_objects.note(session, sourceClipId, id);
                    if (!note)
                        return AutomationResult<MutationResult>(note.getError());
                    sourceNotes.append(note.get().note);
                }
                if (sourceNotes.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                const auto payload = captureNoteTransfer(
                    *static_cast<const SingingClip *>(source.get().clip), sourceNotes);
                return commitTransfer(session, targetClipId, targetStart, payload, validateOnly);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::pasteNotes(const CommandContext &context, const ClipId targetClipId,
                                         const int targetStart,
                                         const NoteTransferPayload &payload) {
        const auto requestFingerprint =
            context.idempotencyKey.isEmpty()
                ? QByteArray{}
                : transferFingerprint(targetClipId, targetStart, payload);
        return m_dispatcher.dispatchIdempotentDocumentCommand(
            OperationIds::notes::duplicate, context, requestFingerprint,
            [this, targetClipId, targetStart, payload](DocumentSession &session,
                                                       const bool validateOnly) {
                return commitTransfer(session, targetClipId, targetStart, payload, validateOnly);
            });
    }

    AutomationResult<MutationResult> NoteAutomationFacade::commitTransfer(
        DocumentSession &session, const ClipId targetClipId, const int targetStart,
        const NoteTransferPayload &payload, const bool validateOnly) {
        auto target = m_objects.singingClip(session, targetClipId);
        if (!target)
            return target.getError();
        if (targetStart < 0 || !validTransferPayload(payload)) {
            return AutomationError::invalidArgument(
                QStringLiteral("payload"), QStringLiteral("Note transfer payload is invalid"));
        }
        const auto translatedPayloadEnd = static_cast<qint64>(targetStart) +
                                          static_cast<qint64>(payload.sourceEnd) -
                                          payload.sourceStart;
        if (translatedPayloadEnd > std::numeric_limits<int>::max()) {
            return AutomationError::invalidArgument(
                QStringLiteral("target_start"),
                QStringLiteral("Translated note range is outside the valid range"));
        }
        const auto delta = static_cast<qint64>(targetStart) - payload.sourceStart;
        for (const auto &draft : payload.notes) {
            const auto translatedStart = static_cast<qint64>(draft.localStart) + delta;
            const auto translatedEnd = translatedStart + draft.length;
            if (translatedStart < 0 || translatedEnd > std::numeric_limits<int>::max()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("target_start"),
                    QStringLiteral("Translated note position is outside the valid range"));
            }
        }
        if (validateOnly)
            return m_committer.preview(session, true,
                                       {
                                           {ObjectKind::Clip, targetClipId.value()}
            });

        auto *targetClip = static_cast<SingingClip *>(target.get().clip);
        const auto parameterReplacements =
            mergeNoteTransferParameters(*targetClip, payload, targetStart);
        std::vector<std::unique_ptr<Note>> ownedNotes;
        QList<Note *> rawNotes;
        QList<ObjectRef> affected;
        QList<CreatedObjectRef> createdObjects;
        ownedNotes.reserve(static_cast<size_t>(payload.notes.size()));
        for (qsizetype index = 0; index < payload.notes.size(); ++index) {
            auto draft = payload.notes.at(index);
            draft.clientRef = QStringLiteral("duplicate_note_%1").arg(index);
            draft.localStart = static_cast<int>(static_cast<qint64>(draft.localStart) + delta);
            auto note = buildNote(draft, targetClip, &createdObjects);
            rawNotes.append(note.get());
            affected.append({ObjectKind::Note, note->id()});
            ownedNotes.push_back(std::move(note));
        }

        auto actions = std::make_unique<NoteTransferActions>();
        actions->insertNotes(rawNotes, targetClip, target.get().track);
        std::vector<std::unique_ptr<Curve>> ownedCurves;
        for (const auto &parameter : parameterReplacements) {
            QList<Curve *> rawCurves;
            rawCurves.reserve(parameter.curves.size());
            for (const auto &draft : parameter.curves) {
                auto curve = buildCurve(draft);
                curve->Curve::setLocalStart(draft.localStart);
                rawCurves.append(curve.get());
                ownedCurves.push_back(std::move(curve));
            }
            actions->replaceParameter(parameter.name, parameter.type, rawCurves, targetClip);
        }
        if (!parameterReplacements.isEmpty())
            affected.append({ObjectKind::Clip, targetClipId.value()});

        auto result =
            m_committer.commit(session, std::move(actions), affected, std::move(createdObjects));
        if (result) {
            for (auto &note : ownedNotes)
                note.release();
        }
        return result;
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::removeNotes(const CommandContext &context, const ClipId clipId,
                                          QList<NoteId> noteIds) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::remove, context,
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
        NoteAutomationFacade::splitNoteAt(const CommandContext &context, const ClipId clipId,
                                          const NoteId noteId, const int localPosition) {
        const auto requestFingerprint = context.idempotencyKey.isEmpty()
                                            ? QByteArray{}
                                            : noteIdsFingerprint(clipId, {noteId}, {localPosition});
        return m_dispatcher.dispatchIdempotentDocumentCommand(
            OperationIds::notes::split_at, context, requestFingerprint,
            [this, clipId, noteId, localPosition](DocumentSession &session,
                                                  const bool validateOnly) {
                auto resolved = m_objects.note(session, clipId, noteId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                const auto originalStart = resolved.get().note->localStart();
                const auto originalEnd = originalStart + resolved.get().note->length();
                if (localPosition <= originalStart || localPosition >= originalEnd) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("local_position"),
                        QStringLiteral("Split position must be inside the note")));
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, true));
                NoteDraftDto draft;
                draft.clientRef = QStringLiteral("split_note");
                draft.localStart = localPosition;
                draft.length = originalEnd - localPosition;
                draft.keyIndex = resolved.get().note->keyIndex();
                draft.centShift = resolved.get().note->centShift();
                draft.language = resolved.get().note->language();
                draft.lyric = QStringLiteral("-");
                draft.pronunciation = resolved.get().note->pronunciation();
                QList<CreatedObjectRef> createdObjects;
                auto note = buildNote(draft, resolved.get().clip, &createdObjects);
                const auto createdId = note->id();
                auto actions = std::make_unique<NoteActions>();
                actions->splitNote(resolved.get().note, note.get(), localPosition - originalStart,
                                   resolved.get().clip);
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
        NoteAutomationFacade::resetPhonemeOffsets(const CommandContext &context,
                                                  const ClipId clipId, QList<NoteId> noteIds) {
        std::sort(noteIds.begin(), noteIds.end(), [](const NoteId left, const NoteId right) {
            return left.value() < right.value();
        });
        const bool duplicates = hasDuplicateIds(noteIds);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::reset_phoneme_offsets, context,
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
                auto *clip = static_cast<SingingClip *>(clipResult.get().clip);
                const auto resetRoots = SingingClipPhonemeNormalizer::collectCascadeResetRoots(
                    *clip, notes, session.model()->timeline());
                QList<NoteId> resetIds;
                resetIds.reserve(resetRoots.size());
                for (const auto *note : resetRoots)
                    resetIds.append(NoteId(note->id()));
                bool changed = false;
                for (const auto *note : resetRoots) {
                    if (note->phonemeOffsetSeq().isEdited()) {
                        changed = true;
                        break;
                    }
                }
                const auto affected = noteRefs(resetIds);
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->resetPhonemeOffsets(resetRoots, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setPhonemeOffsets(const CommandContext &context, const ClipId clipId,
                                                const NoteId noteId, const QList<int> &offsets) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::set_phoneme_offsets, context,
            [this, clipId, noteId, offsets](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.note(session, clipId, noteId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                const auto phonemeCount = resolved.get().note->phonemeNameSeq().result().count();
                if (!offsets.isEmpty() && (offsets.count() != phonemeCount ||
                                           !std::is_sorted(offsets.cbegin(), offsets.cend()))) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("offsets"),
                        QStringLiteral(
                            "Phoneme offsets must match the phoneme count and be sorted")));
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
                if (quantize <= 0 || TimelineSnapUtils::ticksPerWholeNote() % quantize != 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("quantize"), QStringLiteral("Quantize value is invalid")));
                }
                const auto grid = TimelineSnapUtils::quantizeToTicks(quantize);
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

    AutomationResult<MutationResult> NoteAutomationFacade::patchWordProperties(
        const CommandContext &context, const ClipId clipId, QList<NoteWordPatchDto> edits) {
        return patchWordProperties(OperationIds::notes::set_word_properties, context, clipId,
                                   std::move(edits));
    }

    AutomationResult<MutationResult> NoteAutomationFacade::setLyric(const CommandContext &context,
                                                                    const ClipId clipId,
                                                                    const NoteId noteId,
                                                                    const QString &lyric) {
        return patchWordProperties(OperationIds::notes::set_lyric, context, clipId,
                                   {
                                       {.noteId = noteId, .lyric = lyric}
        });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setLanguages(const CommandContext &context, const ClipId clipId,
                                           QList<NoteId> noteIds, const QString &language) {
        QList<NoteWordPatchDto> edits;
        edits.reserve(noteIds.size());
        for (const auto id : noteIds)
            edits.append({.noteId = id, .language = language});
        return patchWordProperties(OperationIds::notes::set_language, context, clipId,
                                   std::move(edits));
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setPronunciation(const CommandContext &context, const ClipId clipId,
                                               const NoteId noteId, const bool originalSource,
                                               const QString &pronunciation) {
        if (!originalSource) {
            auto notes = getNotes(context.expected.documentId, clipId);
            if (!notes)
                return notes.getError();
            const auto found =
                std::find_if(notes.get().cbegin(), notes.get().cend(),
                             [noteId](const auto &note) { return note.id == noteId; });
            if (found == notes.get().cend())
                return AutomationError::notFound({ObjectKind::Note, noteId.value()},
                                                 QStringLiteral("Note was not found"));
            auto value = found->data.pronunciation;
            value.edited = pronunciation;
            return patchWordProperties(OperationIds::notes::set_pronunciation, context, clipId,
                                       {
                                           {.noteId = noteId, .pronunciation = std::move(value)}
            });
        }

        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::notes::set_pronunciation, context,
            [this, clipId, noteId, pronunciation](DocumentSession &session,
                                                  const bool validateOnly) {
                auto resolved = m_objects.note(session, clipId, noteId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                const bool changed = resolved.get().note->pronunciation().original != pronunciation;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Note, noteId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SetOriginalPronunciationActions>(
                    resolved.get().note, resolved.get().clip, pronunciation);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::resetPronunciation(const CommandContext &context, const ClipId clipId,
                                                 const NoteId noteId) {
        auto notes = getNotes(context.expected.documentId, clipId);
        if (!notes)
            return notes.getError();
        const auto found = std::find_if(notes.get().cbegin(), notes.get().cend(),
                                        [noteId](const auto &note) { return note.id == noteId; });
        if (found == notes.get().cend())
            return AutomationError::notFound({ObjectKind::Note, noteId.value()},
                                             QStringLiteral("Note was not found"));
        auto pronunciation = found->data.pronunciation;
        pronunciation.edited.clear();
        return patchWordProperties(OperationIds::notes::reset_pronunciation, context, clipId,
                                   {
                                       {.noteId = noteId, .pronunciation = pronunciation}
        });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::setPhonemes(const CommandContext &context, const ClipId clipId,
                                          const NoteId noteId, const Phonemes &phonemes) {
        return patchWordProperties(OperationIds::notes::set_phonemes, context, clipId,
                                   {
                                       {.noteId = noteId, .phonemes = phonemes}
        });
    }

    AutomationResult<MutationResult>
        NoteAutomationFacade::resetPhonemes(const CommandContext &context, const ClipId clipId,
                                            const NoteId noteId) {
        auto notes = getNotes(context.expected.documentId, clipId);
        if (!notes)
            return notes.getError();
        const auto found = std::find_if(notes.get().cbegin(), notes.get().cend(),
                                        [noteId](const auto &note) { return note.id == noteId; });
        if (found == notes.get().cend())
            return AutomationError::notFound({ObjectKind::Note, noteId.value()},
                                             QStringLiteral("Note was not found"));
        auto phonemes = found->data.phonemes;
        phonemes.nameSeq.edited.clear();
        phonemes.offsetSeq.edited.clear();
        return patchWordProperties(OperationIds::notes::reset_phonemes, context, clipId,
                                   {
                                       {.noteId = noteId, .phonemes = phonemes}
        });
    }

    AutomationResult<MutationResult> NoteAutomationFacade::patchWordProperties(
        const OperationId &operationId, const CommandContext &context, const ClipId clipId,
        QList<NoteWordPatchDto> edits) {
        std::sort(edits.begin(), edits.end(),
                  [](const NoteWordPatchDto &left, const NoteWordPatchDto &right) {
                      return left.noteId.value() < right.noteId.value();
                  });
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context,
            [this, clipId, edits = std::move(edits)](DocumentSession &session,
                                                     const bool validateOnly) {
                QList<NoteId> ids;
                ids.reserve(edits.size());
                for (const auto &edit : edits)
                    ids.append(edit.noteId);
                if (hasDuplicateIds(ids)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("edits"), QStringLiteral("Note IDs must be unique")));
                }
                auto clipResult = m_objects.singingClip(session, clipId);
                if (!clipResult)
                    return AutomationResult<MutationResult>(clipResult.getError());

                QList<Note *> changedNotes;
                QList<Note::WordProperties> properties;
                QList<WordPropertyEditOptions> options;
                QList<NoteId> changedIds;
                for (const auto &edit : edits) {
                    auto resolved = m_objects.note(session, clipId, edit.noteId);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    auto *note = resolved.get().note;
                    const auto previous = Note::WordProperties::fromNote(*note);
                    auto next = previous;
                    if (edit.lyric)
                        next.lyric = edit.lyric->trimmed();
                    if (edit.language)
                        next.language = *edit.language;
                    if (edit.pronunciation)
                        next.pronunciation = *edit.pronunciation;
                    if (edit.pronunciationCandidates)
                        next.pronCandidates = *edit.pronunciationCandidates;
                    if (edit.phonemes)
                        next.phonemes = *edit.phonemes;

                    const bool wordInputChanged =
                        previous.lyric != next.lyric || previous.language != next.language;
                    if (wordInputChanged && !edit.pronunciation)
                        next.pronunciation.edited.clear();
                    if (wordInputChanged && !edit.pronunciationCandidates)
                        next.pronCandidates.clear();
                    if (!edit.phonemes && (wordInputChanged || previous.pronunciation.result() !=
                                                                   next.pronunciation.result())) {
                        next.phonemes = {};
                    }
                    if (wordPropertiesEqual(previous, next))
                        continue;
                    changedNotes.append(note);
                    properties.append(next);
                    options.append(
                        {edit.pronunciation.has_value(), edit.pronunciationCandidates.has_value()});
                    changedIds.append(edit.noteId);
                }
                const auto affected = noteRefs(changedIds);
                if (validateOnly) {
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !changedNotes.isEmpty(), affected));
                }
                if (changedNotes.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<NoteActions>();
                actions->editNotesWordProperties(changedNotes, properties,
                                                 static_cast<SingingClip *>(clipResult.get().clip),
                                                 options);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

} // namespace Automation
