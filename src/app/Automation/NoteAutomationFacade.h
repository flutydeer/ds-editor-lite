#ifndef NOTEAUTOMATIONFACADE_H
#define NOTEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"
#include "DocumentObjectResolver.h"
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

    class NoteAutomationFacade final {
    public:
        NoteAutomationFacade(OperationCatalog &catalog,
                             AutomationDispatcher &dispatcher,
                             CommandCommitter &committer,
                             DocumentObjectResolver &objects);

        AutomationResult<QList<NoteSnapshotDto>> getNotes(const DocumentId &documentId,
                                                          ClipId clipId);
        AutomationResult<MutationResult> insertNotes(const CommandContext &context,
                                                     ClipId clipId,
                                                     const QList<NoteDraftDto> &notes);
        AutomationResult<MutationResult> removeNotes(const CommandContext &context,
                                                     ClipId clipId,
                                                     QList<NoteId> noteIds);
        AutomationResult<MutationResult> moveNotes(const CommandContext &context,
                                                   ClipId clipId,
                                                   QList<NoteId> noteIds,
                                                   int deltaTick,
                                                   int deltaKey);
        AutomationResult<MutationResult> resizeNotesLeft(const CommandContext &context,
                                                         ClipId clipId,
                                                         QList<NoteId> noteIds,
                                                         int deltaTick,
                                                         int minimumLength);
        AutomationResult<MutationResult> resizeNotesRight(const CommandContext &context,
                                                          ClipId clipId,
                                                          QList<NoteId> noteIds,
                                                          int deltaTick,
                                                          int minimumLength);
        AutomationResult<MutationResult> splitNote(const CommandContext &context,
                                                   ClipId clipId,
                                                   NoteId noteId,
                                                   const NoteDraftDto &newNote,
                                                   int newLength);
        AutomationResult<MutationResult> setPhonemeOffsets(const CommandContext &context,
                                                           ClipId clipId,
                                                           NoteId noteId,
                                                           const QList<int> &offsets);
        AutomationResult<MutationResult> quantizeNotes(const CommandContext &context,
                                                       ClipId clipId,
                                                       QList<NoteId> noteIds,
                                                       int quantize,
                                                       bool quantizeStart,
                                                       bool quantizeLength);
        AutomationResult<MutationResult> setWordProperties(
            const CommandContext &context, ClipId clipId, QList<NoteWordEditDto> edits);
        AutomationResult<MutationResult> patchWordProperties(
            const CommandContext &context, ClipId clipId, QList<NoteWordPatchDto> edits);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // NOTEAUTOMATIONFACADE_H
