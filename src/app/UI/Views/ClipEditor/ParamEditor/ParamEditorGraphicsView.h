//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORGRAPHICSVIEW_H
#define PARAMEDITORGRAPHICSVIEW_H

#include "Model/AppModel/Params.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class ParamProperties;
class SingingClip;
class DrawCurve;
class CommonParamEditorView;
class ParamEditorGraphicsScene;

class ParamEditorGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit ParamEditorGraphicsView(ParamEditorGraphicsScene *scene,
                                     const ParamProperties &foregroundProperties,
                                     const ParamProperties &backgroundProperties,
                                     QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);

public slots:
    void setForeground(ParamInfo::Name name, const ParamProperties &properties);
    void setBackground(ParamInfo::Name name, const ParamProperties &properties);
    void updateForeground(Param::Type type, const Param &param) const;
    void updateBackground(Param::Type type, const Param &param) const;

signals:
    void wheelHorScale(QWheelEvent *event);
    void wheelHorScroll(QWheelEvent *event);

private slots:
    void onClipPropertyChanged();
    void onParamChanged(ParamInfo::Name name, Param::Type type) const;
    void onEditCompleted(const QList<DrawCurve *> &curves) const;

private:
    void wheelEvent(QWheelEvent *event) override;
    void moveToNullClipState();
    void moveToSingingClipState(SingingClip *clip);

    static QList<DrawCurve *> getDrawCurves(const QList<Curve *> &curves);

    bool m_debugMode = false;
    SingingClip *m_clip = nullptr;
    CommonParamEditorView *m_foreground = nullptr;
    CommonParamEditorView *m_background = nullptr;

    ParamInfo::Name m_foregroundParam = ParamInfo::Breathiness;
    ParamInfo::Name m_backgroundParam = ParamInfo::Tension;
};

#endif // PARAMEDITORGRAPHICSVIEW_H
