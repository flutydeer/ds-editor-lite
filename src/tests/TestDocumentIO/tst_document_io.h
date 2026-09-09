#pragma once

#include <QObject>

class DocumentIOTests final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void savePointBaseline();
    void importSequenceUndoRedoAndOwnership();
    void historyNamesFollowTranslator();
    void discardedSavedBranchCannotBecomeCleanThroughAddressReuse();
    void guardedTransition_data();
    void guardedTransition();
    void suggestedSavePath_data();
    void suggestedSavePath();
    void existingSavePathIsPreserved();
    void projectPathCaseSensitivity();
    void dirtyRevisionMustBeApprovedAgain();
    void initialUntitledSession();
    void newOpenAndImport();
    void commitObservers();
    void workflowBusyLeaseAcrossReplacement();
    void failureAndCancellationRollback();
    void saveAndSaveAs();
    void generationCleanup();
    void oldIdCollisionAndErrorPriority();
    void dspxAtomicWrite();
    void midiAtomicWrite();
    void dspxTimeSignatureProjectionValidation();
    void dspxRoundTripPreservesEditedPhrase();
    void audioPublicationOverwrite();
    void audioPublicationNoClobber();
    void batchPreparationFailures();
    void selectionAndGeometry();
    void facadeCommitUndoRedo();
    void preparedBatchCommitBoundaries();
};
