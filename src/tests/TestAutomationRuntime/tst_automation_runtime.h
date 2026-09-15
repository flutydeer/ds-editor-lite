#pragma once

#include <QObject>

class AutomationRuntimeTests final : public QObject {
    Q_OBJECT

private slots:
    void documentRoutingAndErrorContext();
    void validateCommitAndStaleRevision();
    void publicBusyAdmissionAndContinuation_data();
    void publicBusyAdmissionAndContinuation();
    void generationReplacementInvalidatesRoutes();
    void headlessCapabilityRouting();
    void invocationSourceCapture();
    void serialReplayAndExplicitOptIn();
    void completedReplayPrecedesWorkflowBusyAdmission();
    void concurrentReplayExecutesOnce();
    void unsuccessfulAttemptsDoNotClaimKeys();
    void documentsAndGenerationsHaveIndependentKeySpaces();
    void facadeWiringUsesOptInOnly();
    void retentionIsBounded();
    void taskManagerStateBoundaries();
    void applicationTaskScope();
    void documentTaskRetention();
    void cancelVersusCommitStress();
    void duplicateCompletionStress();
    void sessionGenerationContract();
    void controlledDispatcherCommitBoundary();
    void committingGenerationReplacement();
    void queuedAndRunningCancellation();
    void completionPermutations();
    void revisionAndObjectDeletion();
    void newAndOpenGenerationReplacement();
    void capacity_data();
    void capacity();
    void leaseCopiesReleaseOnlyOnce();
    void backgroundCapacityIsIndependent();
    void stoppingRejectsWithoutChangingCounters();
    void existingReadUsesCanonicalPath();
    void rejectedRead_data();
    void rejectedRead();
    void nonexistentWriteUsesExistingParent();
    void revocationIsObservedAtReauthorization();
    void fileGrantDoesNotGrantItsDirectory();
    void relativePathIsInvalid();
};
