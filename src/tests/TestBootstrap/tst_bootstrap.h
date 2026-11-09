#pragma once

#include <QObject>

class BootstrapTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidArguments_data();
    void invalidArguments();
    void validPort_data();
    void validPort();
    void flagsDoNotBecomeProjectPaths();
    void delimiterAndPreparseAgree();
    void identicalOptionsCanRepeat();
    void protocolFlagsAreIndependent();
    void runtimeOverridesDoNotMutatePersistence();
    void initTestCase();
    void protocol();
    void identity();
    void automationBootstrap();
    void watcherLimit();
    void initialReadTimeout();
    void coordinator_data();
    void coordinator();
    void forwardRequestReportsPeerFailureAndRecovers_data();
    void forwardRequestReportsPeerFailureAndRecovers();
    void queuedStartupConnectionIsAcknowledged();
    void handoffProtocolRoundTrips();
    void handoffWaitsForMainThreadTakeover();
    void handoffLedgerDedupeCancelAndQuery();
    void handoffCancelAfterDispatchIsNotConfirmed();
    void handoffTerminalLedgerEvictsPredictably();
    void handoffExpiredTerminalQueryReturnsUnknown();
    void handoffPollsHeartbeatWhilePending();
    void watchdogBoundsStalledExit();
    void watchdogIgnoresIdleProcess();
    void watchdogCoversLateQtTeardown();
    void watchdogExitsWithoutLogging();
    void watchdogUsesOneAbsoluteBudget();
    void disabledHardExitIsLeftToTheParent();
    void aboutToQuitFallbackCommitsBeforeBlockingCallbacks();
};
