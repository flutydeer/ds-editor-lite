#ifndef GUICOMPONENTTESTS_H
#define GUICOMPONENTTESTS_H

#include <QObject>

class GuiComponentTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void validColorsAndSubstitution();
    void invalidDefinitions_data();
    void invalidDefinitions();
    void invalidPlaceholders_data();
    void invalidPlaceholders();
    void appearanceThemePreference();
    void bundledStyleSheets();
    void externalThemeRoot_data();
    void externalThemeRoot();
    void bundledThemeLoadingAndFallback();
    void iconPalette_data();
    void iconPalette();
    void systemThemeNotificationsUpdateBoundWidgets();
    void failedThemeKeepsSemanticColors();
    void mixDisplay_data();
    void mixDisplay();
    void selectSinger_data();
    void selectSinger();
    void reachesTarget_data();
    void reachesTarget();
    void replacingTargetChangesDestination();
    void meterPeaksHoldDecayAndKeepClippingLatched();
    void aNewMeterPeakInterruptsDecay();
    void toastContextLifetime_data();
    void toastContextLifetime();
    void tooltipHoverRestoresUpdatedContent();
    void tooltipPointerAnchorsFollowContentAndVisibility_data();
    void tooltipPointerAnchorsFollowContentAndVisibility();
    void seekBarTrackingControlsWhenDraggedValuesCommit_data();
    void seekBarTrackingControlsWhenDraggedValuesCommit();
    void seekBarKeyboardStepsClampAndDoubleClickResets();
    void mixerSliderDragsCommitAndDoubleClickResets_data();
    void mixerSliderDragsCommitAndDoubleClickResets();
    void reorderHandlesTrackRebuiltRowsAndIgnoreBodyDrags();
    void touchClaimsFinishOnSystemCancel_data();
    void touchClaimsFinishOnSystemCancel();
    void touchClaimsKeepControlsIndependentOfPageScrolling();
    void touchFlickContinuesAfterRelease();
    void comboPopupTouchKeepsScrollingAndSelectionSeparate_data();
    void comboPopupTouchKeepsScrollingAndSelectionSeparate();
    void itemViewTouchKeepsScrollingAndSelectionSeparate();
    void pathEditorMovesAndDeletesTheSelectedDirectories();
    void pathEditorInlineEditsCommitOrCancel_data();
    void pathEditorInlineEditsCommitOrCancel();
    void fileSelectorAcceptsTheFirstSuitableLocalDrop_data();
    void fileSelectorAcceptsTheFirstSuitableLocalDrop();
    void overlayScrollMenuNavigatesTheAttachedArea_data();
    void overlayScrollMenuNavigatesTheAttachedArea();
    void expressionSpinBoxMenuEditsTheDisplayedValue_data();
    void expressionSpinBoxMenuEditsTheDisplayedValue();
    void textInputMenusKeepEditingActionsAndCopyAppearance_data();
    void textInputMenusKeepEditingActionsAndCopyAppearance();
};

#endif
