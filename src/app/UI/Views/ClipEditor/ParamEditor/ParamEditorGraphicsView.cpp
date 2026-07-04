//
// Created by fluty on 24-8-21.
//

#include "ParamEditorGraphicsView.h"

#include "ParamEditorGraphicsScene.h"
#include "SpeakerMixEditorView.h"
#include "Controller/ClipController.h"

#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"
#include "UI/Views/Common/TimeGridView.h"
#include "Utils/MathUtils.h"

#include <QKeyEvent>
#include <QWheelEvent>

ParamEditorGraphicsView::ParamEditorGraphicsView(ParamEditorGraphicsScene *scene,
                                                 const ParamProperties &foregroundProperties,
                                                 const ParamProperties &backgroundProperties,
                                                 QWidget *parent)
    : TimeGraphicsView(scene, false, parent) {
    setAttribute(Qt::WA_StyledBackground);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setMinimumHeight(0);
    setScrollBarVisibility(Qt::Horizontal, false);
    setScrollBarVisibility(Qt::Vertical, false);

    m_background = new CommonParamEditorView(backgroundProperties);
    m_background->setZValue(1);
    m_background->setTransparentMouseEvents(true);
    scene->addCommonItem(m_background);

    m_foreground = new CommonParamEditorView(foregroundProperties);
    m_foreground->setZValue(2);
    m_foreground->setTransparentMouseEvents(false);
    scene->addCommonItem(m_foreground);

    m_speakerMixView = new SpeakerMixEditorView;
    m_speakerMixView->setZValue(3);
    m_speakerMixView->setVisible(false);
    scene->addCommonItem(m_speakerMixView);

    connect(m_foreground, &CommonParamEditorView::editCompleted, this,
            &ParamEditorGraphicsView::onEditCompleted);
    connect(m_foreground, &CommonParamEditorView::editStarted, this,
            &ParamEditorGraphicsView::onEditStarted);
    connect(m_foreground, &CommonParamEditorView::editCommitted, this,
            &ParamEditorGraphicsView::onEditCommitted);
    connect(m_foreground, &CommonParamEditorView::editDiscarded, this,
            &ParamEditorGraphicsView::onEditDiscarded);
}

void ParamEditorGraphicsView::setDataContext(SingingClip *clip) {
    clip == nullptr ? moveToNullClipState() : moveToSingingClipState(clip);
}

SpeakerMixEditorView *ParamEditorGraphicsView::speakerMixView() const {
    return m_speakerMixView;
}

void ParamEditorGraphicsView::discardAction() {
    if (m_speakerMixMode || appStatus->currentEditObject != AppStatus::EditObjectType::Param)
        return;
    if (m_foreground)
        m_foreground->discardAction();
}

void ParamEditorGraphicsView::commitAction() {
    if (m_speakerMixMode || appStatus->currentEditObject != AppStatus::EditObjectType::Param)
        return;
    if (m_foreground)
        m_foreground->commitAction();
}

void ParamEditorGraphicsView::setForeground(const ParamInfo::Name name,
                                            const ParamProperties &properties) {
    if (name == ParamInfo::SpeakerMix) {
        m_speakerMixMode = true;
        m_foreground->setVisible(false);
        updateSpeakerMixViewData();
        m_speakerMixView->setVisible(true);
        return;
    }

    if (m_speakerMixMode) {
        m_speakerMixMode = false;
        m_speakerMixView->setVisible(false);
        m_foreground->setVisible(true);
    }

    m_foregroundParam = name;
    m_foreground->setParamProperties(properties);
    updateForeground(Param::Original, *m_clip->params.getParamByName(m_foregroundParam));
    updateForeground(Param::Edited, *m_clip->params.getParamByName(m_foregroundParam));
}

void ParamEditorGraphicsView::setBackground(const ParamInfo::Name name,
                                            const ParamProperties &properties) {
    m_backgroundParam = name;
    m_background->setParamProperties(properties);
    updateBackground(Param::Original, *m_clip->params.getParamByName(m_backgroundParam));
    updateBackground(Param::Edited, *m_clip->params.getParamByName(m_backgroundParam));
}

void ParamEditorGraphicsView::updateForeground(const Param::Type type, const Param &param) const {
    if (type == Param::Original) {
        m_foreground->loadOriginal(
            getDrawCurves(param.curves(m_debugMode ? Param::Edited : Param::Original)));
    } else if (type == Param::Edited) {
        m_foreground->loadEdited(getDrawCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::updateBackground(const Param::Type type, const Param &param) const {
    if (type == Param::Original) {
        m_background->loadOriginal(getDrawCurves(param.curves(Param::Original)));
    } else if (type == Param::Edited) {
        m_background->loadEdited(getDrawCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::onClipPropertyChanged() {
    setSceneLength(m_clip->length());
    setOffset(m_clip->start());
}

void ParamEditorGraphicsView::onParamChanged(const ParamInfo::Name name,
                                             const Param::Type type) const {
    const auto param = m_clip->params.getParamByName(name);
    if (m_foregroundParam == name)
        updateForeground(type, *param);
    if (m_backgroundParam == name)
        updateBackground(type, *param);
}

void ParamEditorGraphicsView::onSpeakerMixChanged() const {
    updateSpeakerMixViewData();
}

void ParamEditorGraphicsView::onEditCompleted(const QList<DrawCurve *> &curves) const {
    QList<Curve *> list;
    for (const auto curve : curves)
        list.append(curve);
    clipController->onParamEdited(m_foregroundParam, list);
}

void ParamEditorGraphicsView::onEditStarted() const {
    if (!m_clip || m_speakerMixMode)
        return;
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Param, m_clip->id(), {}, {}, {},
                                         {m_foregroundParam});
    appStatus->currentEditObject = AppStatus::EditObjectType::Param;
}

void ParamEditorGraphicsView::onEditCommitted() const {
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void ParamEditorGraphicsView::onEditDiscarded() const {
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

bool ParamEditorGraphicsView::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape)
            discardAction();
    } else if (event->type() == QEvent::WindowDeactivate) {
        discardAction();
    }
    return TimeGraphicsView::event(event);
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
    setOffset(0);
    m_background->clearParams();
    m_foreground->clearParams();
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }
    m_clip = nullptr;
    updateSpeakerMixViewData();
}

void ParamEditorGraphicsView::moveToSingingClipState(SingingClip *clip) {
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }

    m_clip = clip;
    setEnabled(true);
    setSceneLength(m_clip->length());
    setOffset(clip->start());
    updateSpeakerMixViewData();

    updateForeground(Param::Original, *m_clip->params.getParamByName(m_foregroundParam));
    updateForeground(Param::Edited, *m_clip->params.getParamByName(
                                        m_debugMode ? m_backgroundParam : m_foregroundParam));
    if (!m_debugMode) {
        updateBackground(Param::Original, *m_clip->params.getParamByName(m_backgroundParam));
        updateBackground(Param::Edited, *m_clip->params.getParamByName(m_backgroundParam));
    }

    connect(clip, &SingingClip::propertyChanged, this,
            &ParamEditorGraphicsView::onClipPropertyChanged);
    connect(clip, &SingingClip::paramChanged, this, &ParamEditorGraphicsView::onParamChanged);
    connect(clip, &SingingClip::speakerMixChanged, this,
            &ParamEditorGraphicsView::onSpeakerMixChanged);
}

void ParamEditorGraphicsView::updateSpeakerMixViewData() const {
    if (!m_speakerMixView)
        return;

    if (!m_clip) {
        m_speakerMixView->setReferenceSpeakers({});
        m_speakerMixView->setSpeakerMixData({});
        return;
    }

    m_speakerMixView->setReferenceSpeakers(m_clip->singerInfo().speakers());
    m_speakerMixView->setSpeakerMixData(m_clip->speakerMixData());
}

QList<DrawCurve *> ParamEditorGraphicsView::getDrawCurves(const QList<Curve *> &curves) {
    QList<DrawCurve *> result;
    for (const auto curve : curves)
        if (curve->type() == Curve::Draw)
            MathUtils::binaryInsert(result, static_cast<DrawCurve *>(curve));
    return result;
}
