#ifndef NOTEAUTOMATIONFACADE_H
#define NOTEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"
#include "DocumentObjectResolver.h"
#include "NoteTransfer.h"
#include "ProjectAutomationDtos.h"

namespace Automation {

    struct NoteSnapshotDto {
        NoteId id;
        ClipId clipId;
        NoteDraftDto data;
    };

    struct NoteWordEditDto {
        NoteId noteId;
        QString lyric;
        QString language;
        Pronunciation pronunciation;
        QStringList pronunciationCandidates;
        Phonemes phonemes;
        bool replacePronunciation = false;
        bool replacePronunciationCandidates = false;
    };

    struct NoteWordPatchDto {
        NoteId noteId;
        std::optional<QString> lyric;
        std::optional<QString> language;
        std::optional<Pronunciation> pronunciation;
        std::optional<QStringList> pronunciationCandidates;
        std::optional<Phonemes> phonemes;
    };

    struct NoteSearchMatchDto {
        NoteId noteId;
        int localStart = 0;
        int length = 0;
        QString lyric;
    };

    class NoteAutomationFacade final {
    public:
        NoteAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                             CommandCommitter &committer, DocumentObjectResolver &objects);

        AutomationResult<QList<NoteSnapshotDto>> getNotes(const DocumentId &documentId,
                                                          ClipId clipId);
        AutomationResult<QList<NoteSearchMatchDto>>
            searchNotes(const DocumentId &documentId, ClipId clipId, const QString &query,
                        const QString &mode, bool caseSensitive, bool regularExpression);
        AutomationResult<MutationResult> insertNotes(const CommandContext &context, ClipId clipId,
                                                     const QList<NoteDraftDto> &notes);
        AutomationResult<MutationResult> duplicateNotes(const CommandContext &context,
                                                        ClipId sourceClipId, QList<NoteId> noteIds,
                                                        ClipId targetClipId, int targetStart);
        AutomationResult<MutationResult> pasteNotes(const CommandContext &context,
                                                    ClipId targetClipId, int targetStart,
                                                    const NoteTransferPayload &payload);
        AutomationResult<MutationResult> removeNotes(const CommandContext &context, ClipId clipId,
                                                     QList<NoteId> noteIds);
        AutomationResult<MutationResult> moveNotes(const CommandContext &context, ClipId clipId,
                                                   QList<NoteId> noteIds, int deltaTick,
                                                   int deltaKey);
        AutomationResult<MutationResult> resizeNotesLeft(const CommandContext &context,
                                                         ClipId clipId, QList<NoteId> noteIds,
                                                         int deltaTick, int minimumLength);
        AutomationResult<MutationResult> resizeNotesRight(const CommandContext &context,
                                                          ClipId clipId, QList<NoteId> noteIds,
                                                          int deltaTick, int minimumLength);
        AutomationResult<MutationResult> splitNote(const CommandContext &context, ClipId clipId,
                                                   NoteId noteId, const NoteDraftDto &newNote,
                                                   int newLength);
        AutomationResult<MutationResult> splitNoteAt(const CommandContext &context, ClipId clipId,
                                                     NoteId noteId, int localPosition);
        AutomationResult<MutationResult> setPhonemeOffsets(const CommandContext &context,
                                                           ClipId clipId, NoteId noteId,
                                                           const QList<int> &offsets);
        AutomationResult<MutationResult> quantizeNotes(const CommandContext &context, ClipId clipId,
                                                       QList<NoteId> noteIds, int quantize,
                                                       bool quantizeStart, bool quantizeLength);
        AutomationResult<MutationResult> setWordProperties(const CommandContext &context,
                                                           ClipId clipId,
                                                           QList<NoteWordEditDto> edits);
        AutomationResult<MutationResult> patchWordProperties(const CommandContext &context,
                                                             ClipId clipId,
                                                             QList<NoteWordPatchDto> edits);
        AutomationResult<MutationResult> setLyric(const CommandContext &context, ClipId clipId,
                                                  NoteId noteId, const QString &lyric);
        AutomationResult<MutationResult> setLanguages(const CommandContext &context, ClipId clipId,
                                                      QList<NoteId> noteIds,
                                                      const QString &language);
        AutomationResult<MutationResult> setPronunciation(const CommandContext &context,
                                                          ClipId clipId, NoteId noteId,
                                                          bool originalSource,
                                                          const QString &pronunciation);
        AutomationResult<MutationResult> resetPronunciation(const CommandContext &context,
                                                            ClipId clipId, NoteId noteId);
        AutomationResult<MutationResult> setPhonemes(const CommandContext &context, ClipId clipId,
                                                     NoteId noteId, const Phonemes &phonemes);
        AutomationResult<MutationResult> resetPhonemes(const CommandContext &context, ClipId clipId,
                                                       NoteId noteId);

    private:
        AutomationResult<MutationResult> commitTransfer(DocumentSession &session,
                                                        ClipId targetClipId, int targetStart,
                                                        const NoteTransferPayload &payload,
                                                        bool validateOnly);
        AutomationResult<MutationResult> patchWordProperties(const OperationId &operationId,
                                                             const CommandContext &context,
                                                             ClipId clipId,
                                                             QList<NoteWordPatchDto> edits);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // NOTEAUTOMATIONFACADE_H
