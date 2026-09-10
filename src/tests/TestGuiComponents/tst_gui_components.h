#ifndef GUICOMPONENTTESTS_H
#define GUICOMPONENTTESTS_H

#include <QObject>

class GuiComponentTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void validColorsAndSubstitution();
    void invalidDefinitions();
    void invalidPlaceholders();
    void appearanceThemePreference();
    void bundledStyleSheets();
    void externalThemeRoot();
    void bundledThemeLoadingAndFallback();
    void iconPalette_data();
    void iconPalette();
    void failedThemeKeepsSemanticColors();
    void mixDisplay_data();
    void mixDisplay();
    void selectSinger_data();
    void selectSinger();
    void reachesTarget_data();
    void reachesTarget();
    void replacingTargetChangesDestination();
    void toastContextLifetime_data();
    void toastContextLifetime();
    void seekBarTrackingControlsWhenDraggedValuesCommit_data();
    void seekBarTrackingControlsWhenDraggedValuesCommit();
    void seekBarKeyboardStepsClampAndDoubleClickResets();
    void mixerSliderReleaseEndsPreview_data();
    void mixerSliderReleaseEndsPreview();
};

#endif
