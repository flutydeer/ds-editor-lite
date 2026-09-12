#pragma once

#include <QObject>

#include <QtTest/QTest>
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>

InferSpeakerMix twoSpeakerMix(const QString &first = QStringLiteral("S1"),
                              const QString &second = QStringLiteral("S2"));

class VoiceAndInferenceTests final : public QObject {
    Q_OBJECT

private slots:
    void speakerMixWeightConversions();
    void speakerMixOverlappingSplitResolution();
    void speakerMixNormalizeSpeakerMixData();
    void speakerMixDynamicStatePredicates();
    void speakerMixStaticAndFixedInferenceMix();
    void speakerMixDynamicInferenceMix();
    void voiceContextNotificationsContainAtomicBeforeAndAfter();
    void voiceContextInheritanceOnlyChangePreservesInferenceInput();
    void voiceContextInheritedClipsFollowTrackWhileOwnVoiceRemains();
    void voiceContextSpeakerMetadataCopiesAreIndependent();
    void voiceContextSingerCapabilityCopiesAreIndependent();
    void inputConversionPieceSpeakerMix_data();
    void inputConversionPieceSpeakerMix();
    void inputConversionWordSpeakerMapping_data();
    void inputConversionWordSpeakerMapping();
    void inputConversionFrameSpeakerMapping_data();
    void inputConversionFrameSpeakerMapping();
    void inputConversionParameterRetake_data();
    void inputConversionParameterRetake();
    void inputConversionVocoderPitchStaysHostOnlyAndAffectsCacheKey();
    void inputConversionPhonemeTone_data();
    void inputConversionPhonemeTone();
    void inputConversionPhonemeSpeakerMixUsesMappedWeights();
    void inputWordsKeepSlursAndPreutteranceAcrossGaps_data();
    void inputWordsKeepSlursAndPreutteranceAcrossGaps();
    void inputConversionAvailableSpeakersFillEmptyMix_data();
    void inputConversionAvailableSpeakersFillEmptyMix();
    void inputConversionAvailableMixIsPreserved();
    void inputConversionUnavailableSourcesAreRemoved();
    void inputConversionUnusableMixFallsBackToAvailableSpeaker_data();
    void inputConversionUnusableMixFallsBackToAvailableSpeaker();
    void inputConversionUnavailablePrimaryIsReplaced();
    void inputConversionUnavailableFallbackIsReplaced();
    void inputConversionUnresolvedSpeakerMetadataPreservesInput_data();
    void inputConversionUnresolvedSpeakerMetadataPreservesInput();
    void inputConversionSurvivingMixCurvesAreRenormalized();
    void singerSessionCacheRetainsOnlySelectedSingers();
    void singerSessionCacheLatestRetainedIdentifiers();
    void singerSessionCacheActiveCallerAndStaleReplacement();
    void singerSessionCacheLeastRecentlyUsedEviction();
    void singerSessionCacheIdleEvictionAndActiveReuse();
    void inferCacheScanAndClean();
    void inferCacheClearRegisteredFiles();
};
