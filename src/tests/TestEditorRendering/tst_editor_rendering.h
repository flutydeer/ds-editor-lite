#ifndef EDITORRENDERINGTESTS_H
#define EDITORRENDERINGTESTS_H

#include <QObject>

class EditorRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void coverageAndAtlasRows();
    void fractionalDprAndTextureReplacement();
    void subpixelPhaseCache();
    void cameraRelativeAlignment();
    void windowRelativeAlignment();
    void drawOrderPreservesOcclusion();
    void blendColorsRemainDistinct();
    void currentFramePagesAreNotEvicted();
    void minimumItemGeometry();
    void triangleClipping();
    void roundedStrokeClipping();
    void hairlineDeduplication();
    void shortStrokeEndCaps();
    void subpixelOverlayCoverage();
    void antialiasedCircle();
    void amplitudeMapping();
    void filledPeaks();
    void verticalPeaks();
    void curveAndSampleDots();
    void transformedCurve();
};

#endif
