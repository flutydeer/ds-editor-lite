#pragma once

#include <QObject>

class AudioAssetsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void hashSnapshot();
    void hitRelative();
    void hitSibling();
    void hitUnconfirmed();
    void hashMismatch();
    void currentDirectoryDecoy();
    void directoryCandidateRejected();
    void derivedAudioWritebacks();
    void samePathSourceReplacement();
    void resolvedPathValidation();
    void samePathResolutionNotification();
    void samePathRelinkNotification();
    void resolvedPathSourceFailureWins();
    void relinkHistoryNotifications();
    void resolveDecodeTaskProtocol();
    void deletedAudioTargetTerminalState();
    void hashDecodeOrdering_data();
    void hashDecodeOrdering();
    void openSources_data();
    void openSources();
    void replacementBeforeDeferredStart();
    void mixedImportSources();
    void resolutionRetryPreservesSource();
    void relocatedDecodeNotification();
    void decodedWaveformRetainsPeaks_data();
    void decodedWaveformRetainsPeaks();
    void waveformSamplingFollowsZoom_data();
    void waveformSamplingFollowsZoom();
    void waveformSamplingRefreshesSourceAndTempo();
};
