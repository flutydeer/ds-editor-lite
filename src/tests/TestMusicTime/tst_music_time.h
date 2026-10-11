#pragma once

#include <QObject>

class MusicTimeTests final : public QObject {
    Q_OBJECT

private slots:
    void musicTimelineSinglePointMatchesLegacyConverter();
    void musicTimelineDegenerateEquivalence();
    void musicTimelineTickMsRoundTrip();
    void musicTimelineTempoQueries();
    void musicTimelineBarTickMapping();
    void musicTimelineTickTimeRoundTrip();
    void musicTimelineZeroPointInvariant();
    void musicTimelineMutationApi();
    void musicTimelineExtremeValues();
    void musicTimelineBarAnchoredSnapping();
    void musicTimelineMusicTimeStrings();
    void audioAnchorDegenerateEquivalence();
    void audioAnchorRoundTripIdentity();
    void audioAnchorRealTempoChange();
    void audioAnchorCompensationProperty();
    void audioAnchorPropertiesRoundTrip();
    void audioAnchorMovePreservesTruth();
    void audioAnchorDragPreviewMatchesCommit();
};
