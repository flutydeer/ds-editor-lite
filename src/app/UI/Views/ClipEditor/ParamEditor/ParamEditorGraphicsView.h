//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORGRAPHICSVIEW_H
#define PARAMEDITORGRAPHICSVIEW_H

#include "Model/AppModel/Params.h"
#include "UI/Views/Common/TimeGraphicsView.h"


class SingingClip;
class DrawCurve;
class CommonParamEditorView;
class ParamEditorGraphicsScene;

class ParamEditorGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit ParamEditorGraphicsView(ParamEditorGraphicsScene *scene, QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);

public slots:
    void setForegroundParam(Param::Type type, const Param &param) const;
    void setBackgroundParam(Param::Type type, const Param &param);

signals:
    void wheelHorScale(QWheelEvent *event);
    void wheelHorScroll(QWheelEvent *event);

private slots:
    void onClipPropertyChanged() const;

private:
    void wheelEvent(QWheelEvent *event) override;
    void moveToNullClipState();
    void moveToSingingClipState(SingingClip *clip);

    static QList<DrawCurve *> getDrawCurves(const QList<Curve *> &curves);

    SingingClip *m_clip = nullptr;
    CommonParamEditorView *m_foreground;
    CommonParamEditorView *m_background;
};

#endif // PARAMEDITORGRAPHICSVIEW_H
