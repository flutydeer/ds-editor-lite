#ifndef PARAMEDITORGRAPHICSVIEW_H
#define PARAMEDITORGRAPHICSVIEW_H

#include "Interface/IAtomicAction.h"
#include "ParamEditorEditMode.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include <lite/ProjectModel/AppModel/Params.h>
#include "UI/Views/Common/TimeGraphicsView.h"

class ParamProperties;
class SingingClip;
class DrawCurve;
class AnchorCurve;
class CommonParamEditorView;
class ParamEditorGraphicsScene;
class ParamAnchorOverlayView;
class SpeakerMixEditorView;

class ParamEditorGraphicsView final : public TimeGraphicsView, public IAtomicAction {
    Q_OBJECT
    Q_PROPERTY(QColor paramGraduateColor READ paramGraduateColor WRITE setParamGraduateColor)
    Q_PROPERTY(QColor paramOriginalCurveColor READ paramOriginalCurveColor WRITE
                   setParamOriginalCurveColor)
    Q_PROPERTY(
        QColor paramEditedCurveColor READ paramEditedCurveColor WRITE setParamEditedCurveColor)
    Q_PROPERTY(bool paramUseTrackColorForEditedCurve READ paramUseTrackColorForEditedCurve WRITE
                   setParamUseTrackColorForEditedCurve)
    Q_PROPERTY(QColor paramBackgroundLayerColor READ paramBackgroundLayerColor WRITE
                   setParamBackgroundLayerColor)
    Q_PROPERTY(QColor anchorColor READ anchorColor WRITE setAnchorColor)
    Q_PROPERTY(QColor anchorSelectedColor READ anchorSelectedColor WRITE setAnchorSelectedColor)
    Q_PROPERTY(QColor anchorCurveColor READ anchorCurveColor WRITE setAnchorCurveColor)
    Q_PROPERTY(QColor anchorPreviewColor READ anchorPreviewColor WRITE setAnchorPreviewColor)
    Q_PROPERTY(QColor speakerMixTextColor READ speakerMixTextColor WRITE setSpeakerMixTextColor)
    Q_PROPERTY(QColor speakerMixKeyframeLineColor READ speakerMixKeyframeLineColor WRITE
                   setSpeakerMixKeyframeLineColor)
    Q_PROPERTY(QColor speakerMixSelectedDotColor READ speakerMixSelectedDotColor WRITE
                   setSpeakerMixSelectedDotColor)
    Q_PROPERTY(QColor speakerMixSelectionBorderColor READ speakerMixSelectionBorderColor WRITE
                   setSpeakerMixSelectionBorderColor)
    Q_PROPERTY(QColor speakerMixSelectionFillColor READ speakerMixSelectionFillColor WRITE
                   setSpeakerMixSelectionFillColor)

public:
    explicit ParamEditorGraphicsView(ParamEditorGraphicsScene *scene,
                                     const ParamProperties &foregroundProperties,
                                     const ParamProperties &backgroundProperties,
                                     QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);
    [[nodiscard]] SpeakerMixEditorView *speakerMixView() const;
    void discardAction() override;
    void commitAction() override;
    void setEditMode(ParamEditorEditMode mode);
    [[nodiscard]] ParamEditorEditMode editMode() const;
    void setForegroundBaseCurveVisible(bool visible);
    void setBackgroundBaseCurveVisible(bool visible);

public slots:
    void deleteSelection();
    void setForeground(ParamInfo::Name name, const ParamProperties &properties);
    void setBackground(ParamInfo::Name name, const ParamProperties &properties);
    void updateForeground(Param::Type type, const Param &param);
    void updateBackground(Param::Type type, const Param &param) const;

signals:
    void wheelHorScale(QWheelEvent *event);
    void wheelHorScroll(QWheelEvent *event);

private slots:
    void onClipPropertyChanged();
    void onParamChanged(ParamInfo::Name name, Param::Type type);
    void onSpeakerMixChanged() const;
    void onEditStarted();
    void onEditCommitted();
    void onEditDiscarded();
    void onEditCompleted(const QList<DrawCurve *> &curves);
    void showAnchorContextMenu(QPointF scenePos, QPoint screenPos);

private:
    bool event(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                               Qt::KeyboardModifiers modifiers) override;
    void moveToNullClipState();
    void moveToSingingClipState(SingingClip *clip);
    void updateSpeakerMixViewData() const;

    [[nodiscard]] QColor paramGraduateColor() const;
    void setParamGraduateColor(const QColor &color);
    [[nodiscard]] QColor paramOriginalCurveColor() const;
    void setParamOriginalCurveColor(const QColor &color);
    [[nodiscard]] QColor paramEditedCurveColor() const;
    void setParamEditedCurveColor(const QColor &color);
    [[nodiscard]] bool paramUseTrackColorForEditedCurve() const;
    void setParamUseTrackColorForEditedCurve(bool on);
    [[nodiscard]] QColor paramBackgroundLayerColor() const;
    void setParamBackgroundLayerColor(const QColor &color);
    [[nodiscard]] QColor anchorColor() const;
    void setAnchorColor(const QColor &color);
    [[nodiscard]] QColor anchorSelectedColor() const;
    void setAnchorSelectedColor(const QColor &color);
    [[nodiscard]] QColor anchorCurveColor() const;
    void setAnchorCurveColor(const QColor &color);
    [[nodiscard]] QColor anchorPreviewColor() const;
    void setAnchorPreviewColor(const QColor &color);
    [[nodiscard]] QColor speakerMixTextColor() const;
    void setSpeakerMixTextColor(const QColor &color);
    [[nodiscard]] QColor speakerMixKeyframeLineColor() const;
    void setSpeakerMixKeyframeLineColor(const QColor &color);
    [[nodiscard]] QColor speakerMixSelectedDotColor() const;
    void setSpeakerMixSelectedDotColor(const QColor &color);
    [[nodiscard]] QColor speakerMixSelectionBorderColor() const;
    void setSpeakerMixSelectionBorderColor(const QColor &color);
    [[nodiscard]] QColor speakerMixSelectionFillColor() const;
    void setSpeakerMixSelectionFillColor(const QColor &color);

    static QList<DrawCurve *> getDrawCurves(const QList<Curve *> &curves);
    static QList<AnchorCurve *> getAnchorCurves(const QList<Curve *> &curves);
    bool beginAnchorEditSession();
    void publishAnchors(const QList<AnchorCurve *> &curves);
    void finishAnchorEditSession(AnchorEditor::EditFinishReason reason);
    void onAnchorStateChanged();

    bool m_debugMode = false;
    bool m_speakerMixMode = false;
    SingingClip *m_clip = nullptr;
    CommonParamEditorView *m_foreground = nullptr;
    CommonParamEditorView *m_background = nullptr;
    SpeakerMixEditorView *m_speakerMixView = nullptr;
    ParamAnchorOverlayView *m_anchorOverlay = nullptr;
    AnchorEditor::AnchorEditController m_anchorController;
    quint64 m_drawSessionId = 0;
    quint64 m_anchorSessionId = 0;
    quint64 m_renderedAnchorRevision = 0;
    ParamEditorEditMode m_editMode = ParamEditorEditMode::Draw;

    ParamInfo::Name m_foregroundParam = ParamInfo::Breathiness;
    ParamInfo::Name m_backgroundParam = ParamInfo::Tension;
};

#endif // PARAMEDITORGRAPHICSVIEW_H
