#ifndef EDITORINTERACTIONTESTS_H
#define EDITORINTERACTIONTESTS_H

#include <QObject>
#include <memory>

class GuiAppFixture;

class EditorInteractionTests final : public QObject {
    Q_OBJECT

public:
    EditorInteractionTests();
    ~EditorInteractionTests() override;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void noView();
    void commandCapabilities();
    void modeAwareCommandRouting();
    void forwardingAndSnapshots();
    void activePanels();
    void interactionRouting();
    void panelVisibilityRouting();
    void visibleExecutesImmediately();
    void scrollRequiredExecutesOnSecondRequest();
    void redoUsesTwoPhases();
    void contextSwitchExecutesOnSecondRequest();
    void directionChangeClearsPending();
    void historyChangeInvalidatesPending();
    void fallbacksAndEditGuard();
    void viewportMargin();
    void viewportResizeClamping();
    void pianoViewportZoomAnchor();
    void focusReveal();
    void repeatedBoundaryScrollDoesNotNotify();
    void animatedAndImmediateViewportDestinations();
    void legacyWheelZoomPreservesTheInputAnchor_data();
    void legacyWheelZoomPreservesTheInputAnchor();
    void legacyViewportAnimationCanBeFinishedOrInterrupted_data();
    void legacyViewportAnimationCanBeFinishedOrInterrupted();
    void outsideHotZone();
    void edgeDirections();
    void speedSaturation();
    void cornerAndDisabledAxes();
    void subpixelAccumulation();
    void fractionalRemainders();
    void pressDeadZone();
    void pressAwareHorizontalAxis();
    void pressAwareDisabledAxes();
    void pressOutsideHotZone();
    void pressAwareStep();
    void dragSessionLifecycle();
    void pointerClamping();
    void canonicalNoteOrder();
    void drawAndResizeGeometry();
    void lyricVisibility();
    void orderedSelection();
    void clickAndDragSelection();
    void contextMenuSelection();
    void resizeHitTesting();
    void clipSelection();
    void singingClipRightResize();
    void pasteExtendsVisibleRange();
    void trimmedClipRetainsTailRoom();
    void clipResizeBounds();
    void minimumLengthAndContentBounds();
    void clipPreviewLayout();
    void projectedNotePreview();
    void audioMovePreservesRealTimeWindow();
    void audioLeftTrimPreservesMaterialOriginAndRightEdge();
    void audioRightTrimStopsAtMaterialBoundary();
    void audioResizeAcrossOppositeEdge_data();
    void audioResizeAcrossOppositeEdge();
    void sceneAttachment();
    void overlayStartupAndZoom();
    void rhiScrollbars();
    void touchpadAndWheelDelta();
    void discreteWheelWithPixelDelta();
    void horizontalAndShiftGestures();
    void fractionalAndReversedWheelMotion();
    void pendingWheelTargetsRespectBounds();
    void stoppingWheelMotionPreservesExternalInput();
    void wheelAndNativeZoomAnchors();
    void controlWheelPolicies();
    void editingFocusProtectsTextInput();
    void applicationShortcutOverridesTools_data();
    void applicationShortcutOverridesTools();
    void applicationShortcutPreservesTextInput();
    void applicationShortcutPreservesOtherWindows_data();
    void applicationShortcutPreservesOtherWindows();
    void disablingShortcutRestoresButtonInput();
    void leavingMenuClearsPastePreview();
    void trackListDragReordersOrCancels_data();
    void trackListDragReordersOrCancels();
    void speakerMixDragKeepsWeightsWithTheirSources_data();
    void speakerMixDragKeepsWeightsWithTheirSources();
    void speakerMixSourceChoicePreservesWeightsAndUpdatesTags();
    void lyricRuleDragPreservesEditsAndChangesPriority_data();
    void lyricRuleDragPreservesEditsAndChangesPriority();

private:
    std::unique_ptr<GuiAppFixture> application;
};

#endif
