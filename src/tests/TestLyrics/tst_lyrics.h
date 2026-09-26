#pragma once

#include <QObject>

class LyricsTests final : public QObject {
    Q_OBJECT

private slots:
    void lrcTimestamps_data();
    void lrcTimestamps();
    void lrcMetadataRepeatedLinesAndSeeking();
    void lrcFailedReloadClearsPreviousDocument();
    void lyricSplittingModesPreserveLines();
    void lyricRulesStableOrderPersistence();
    void lyricRulesLegacyOrderMigration();
    void lyricRulesStableAutomationRuleIdMigration();
    void lyricRulesRuntimeOrder();
    void lyricRulesSplitterPreservesMixedLanguageText();
    void lyricRulesSplitterEnabledRules_data();
    void lyricRulesSplitterEnabledRules();
    void lyricRulesSplitterRulePriority_data();
    void lyricRulesSplitterRulePriority();
    void lyricRulesSplitterEmptyMatchDoesNotDuplicateText();
    void syllabificationRanges();
    void syllabificationSlicerSyllableAssignment();
    void syllabificationSlicerAssignmentRecovery();
    void syllabificationStorageAndInferenceRoundTrip();
    void syllabificationDetachedSyllabificationNotesStayOrphaned();
    void syllabificationBuildWordsRejectsPendingOffsets();
    void syllabificationEditingEligibility();
    void syllabificationRelativeTimingChangeInvalidatesEditedOffsets();
    void syllabificationTempoAwareWordState();
    void syllabificationCascadeResetClosure();
    void syllabificationCascadeResetStopsWithoutOverlap();
    void syllabificationCascadeResetOnlyEdited();
    void syllabificationCascadeResetMultiStep();
    void syllabificationCascadeResetStopsAtGap();
    void syllabificationCascadeResetWordEndThroughMembers();
    void syllabificationCascadeResetSkippedWithoutBaseline();
    void wordPropertyCascadeLyricChangeResetsPronunciationAndPhonemes();
    void wordPropertyCascadePronunciationChangeResetsPhonemes();
    void wordPropertyCascadeUnchangedUpperPropertiesPreservePhonemes();
    void wordPropertyCascadeEquivalentPronunciationPreservesPhonemes();
    void wordPropertyCascadeEqualFillLyricReplacementsArePreserved();
};
