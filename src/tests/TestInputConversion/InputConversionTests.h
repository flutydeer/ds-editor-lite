#pragma once

#include <QtTest/QTest>
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>

InferSpeakerMix twoSpeakerMix(const QString &first = QStringLiteral("S1"),
                              const QString &second = QStringLiteral("S2"));

class InputConversionTests final : public QObject {
    Q_OBJECT
private slots:
    void pieceSpeakerMix_data();
    void pieceSpeakerMix();
    void wordSpeakerMapping_data();
    void wordSpeakerMapping();
    void frameSpeakerMapping_data();
    void frameSpeakerMapping();
    void parameterRetake_data();
    void parameterRetake();
    void vocoderPitchStaysHostOnlyAndAffectsCacheKey();
    void phonemeTone_data();
    void phonemeTone();
    void phonemeSpeakerMixUsesMappedWeights();
    void availableSpeakersFillEmptyMix_data();
    void availableSpeakersFillEmptyMix();
    void availableMixIsPreserved();
    void unavailableSourcesAreRemoved();
    void unusableMixFallsBackToAvailableSpeaker_data();
    void unusableMixFallsBackToAvailableSpeaker();
    void unavailablePrimaryIsReplaced();
    void unavailableFallbackIsReplaced();
    void unresolvedSpeakerMetadataPreservesInput_data();
    void unresolvedSpeakerMetadataPreservesInput();
    void survivingMixCurvesAreRenormalized();
};
