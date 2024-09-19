//
// Created by fluty on 24-8-21.
//

#include "ParamEditorGraphicsView.h"

#include "ParamEditorGraphicsScene.h"
#include "Controller/ClipController.h"

#include "Model/AppModel/SingingClip.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"
#include "UI/Views/Common/TimeGridGraphicsItem.h"
#include "Utils/MathUtils.h"

#include <QWheelEvent>

ParamEditorGraphicsView::ParamEditorGraphicsView(ParamEditorGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent) {
    setAttribute(Qt::WA_StyledBackground);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setMinimumHeight(0);
    setScrollBarVisibility(Qt::Horizontal, false);
    setScrollBarVisibility(Qt::Vertical, false);

    // auto grid = new TimeGridGraphicsItem;
    // grid->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    // setGridItem(grid);

    m_background = new CommonParamEditorView;
    m_background->setZValue(1);
    m_background->setTransparentMouseEvents(true);
    scene->addCommonItem(m_background);

    m_foreground = new CommonParamEditorView;
    m_foreground->setZValue(2);
    m_foreground->setTransparentMouseEvents(false);
    scene->addCommonItem(m_foreground);

    connect(m_foreground, &CommonParamEditorView::editCompleted, this,
            &ParamEditorGraphicsView::onEditCompleted);
}

void ParamEditorGraphicsView::setDataContext(SingingClip *clip) {
    clip == nullptr ? moveToNullClipState() : moveToSingingClipState(clip);
}

void ParamEditorGraphicsView::setForeground(ParamInfo::Name name) {
    m_foregroundParam = name;
    updateForeground(Param::Original, *m_clip->params.getParamByName(m_foregroundParam));
    updateForeground(Param::Edited, *m_clip->params.getParamByName(m_foregroundParam));
}

void ParamEditorGraphicsView::setBackground(ParamInfo::Name name) {
    m_backgroundParam = name;
    updateBackground(Param::Original, *m_clip->params.getParamByName(m_backgroundParam));
    updateBackground(Param::Edited, *m_clip->params.getParamByName(m_backgroundParam));
}

void ParamEditorGraphicsView::updateForeground(Param::Type type, const Param &param) const {
    if (type == Param::Original) {
        m_foreground->loadOriginal(getDrawCurves(param.curves(Param::Original)));
    } else if (type == Param::Edited) {
        m_foreground->loadEdited(getDrawCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::updateBackground(Param::Type type, const Param &param) const {
    if (type == Param::Original) {
        m_background->loadOriginal(getDrawCurves(param.curves(Param::Original)));
    } else if (type == Param::Edited) {
        m_background->loadEdited(getDrawCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::onClipPropertyChanged() const {
    setSceneLength(m_clip->length());
}

void ParamEditorGraphicsView::onParamChanged(ParamInfo::Name name, Param::Type type) const {
    auto param = m_clip->params.getParamByName(name);
    if (m_foregroundParam == name)
        updateForeground(type, *param);
    if (m_backgroundParam == name)
        updateBackground(type, *param);
}

void ParamEditorGraphicsView::onEditCompleted(const QList<DrawCurve *> &curves) const {
    QList<Curve *> list;
    for (const auto curve : curves)
        list.append(curve);
    clipController->onParamEdited(m_foregroundParam, list);
}

void ParamEditorGraphicsView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        emit wheelHorScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        emit wheelHorScroll(event);
    }
}

void ParamEditorGraphicsView::moveToNullClipState() {
    setEnabled(false);
    m_background->clearParams();
    m_foreground->clearParams();
    // while (m_notes.count() > 0)
    //     handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }
    m_clip = nullptr;
}

void ParamEditorGraphicsView::moveToSingingClipState(SingingClip *clip) {
    // while (m_notes.count() > 0)
    //     handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }

    m_clip = clip;
    setEnabled(true);
    setSceneLength(m_clip->length());

    // if (clip->notes().count() > 0) {
    //     for (const auto note : clip->notes())
    //         handleNoteInserted(note);
    // }

    updateForeground(Param::Original, *m_clip->params.getParamByName(m_foregroundParam));
    updateForeground(Param::Edited, *m_clip->params.getParamByName(m_foregroundParam));
    updateBackground(Param::Original, *m_clip->params.getParamByName(m_backgroundParam));
    updateBackground(Param::Edited, *m_clip->params.getParamByName(m_backgroundParam));

    connect(clip, &SingingClip::propertyChanged, this,
            &ParamEditorGraphicsView::onClipPropertyChanged);
    // connect(clip, &SingingClip::noteChanged, this, &PianoRollGraphicsViewPrivate::onNoteChanged);
    connect(clip, &SingingClip::paramChanged, this, &ParamEditorGraphicsView::onParamChanged);
}

QList<DrawCurve *> ParamEditorGraphicsView::getDrawCurves(const QList<Curve *> &curves) {
    QList<DrawCurve *> result;
    for (const auto curve : curves)
        if (curve->type() == Curve::Draw)
            MathUtils::binaryInsert(result, reinterpret_cast<DrawCurve *>(curve));
    return result;
}