#include "ParamEditorGraphicsView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamAnchorOverlayView.h"
#include "SpeakerMixEditorView.h"
#include "Controller/ClipController.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"
#include "UI/Views/Common/TimeGridView.h"
#include <lite/Support/MathUtils.h>

#include <QKeyEvent>
#include <QActionGroup>
#include <QWheelEvent>
#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>

ParamEditorGraphicsView::ParamEditorGraphicsView(ParamEditorGraphicsScene *scene,
                                                 const ParamProperties &foregroundProperties,
                                                 const ParamProperties &backgroundProperties,
                                                 QWidget *parent)
    : TimeGraphicsView(scene, true, parent) {
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

    m_anchorController.setCoordinateMapper({
        [this](const double x) { return qRound(sceneXToTick(x)); },
        [this](const int tick) { return tickToSceneX(tick); },
        [this](const double y) { return qRound(m_foreground->valueAtSceneY(y)); },
        [this](const int value) { return m_foreground->sceneYForValue(value); },
    });
    m_anchorController.setHostCallbacks({
        [this] { return beginAnchorEditSession(); },
        [this](const QList<AnchorCurve *> &curves) { publishAnchors(curves); },
        [this](const AnchorEditor::EditFinishReason reason) { finishAnchorEditSession(reason); },
        [this] { onAnchorStateChanged(); },
    });
    m_anchorOverlay = new ParamAnchorOverlayView(
        &m_anchorController,
        [this](const double value) { return m_foreground->sceneYForValue(value); },
        [this](const double y) { return m_foreground->valueAtSceneY(y); });
    m_anchorOverlay->setOverlayState(&m_anchorController.state());
    m_anchorOverlay->setZValue(2.5);
    scene->addCommonItem(m_anchorOverlay);

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
    connect(m_anchorOverlay, &ParamAnchorOverlayView::autoScrollRequested, this,
            [this](const Qt::Orientations axes) { armEdgeAutoScroll(axes); });
    connect(m_anchorOverlay, &ParamAnchorOverlayView::autoScrollStopped, this,
            [this] { disarmEdgeAutoScroll(); });
    connect(m_anchorOverlay, &ParamAnchorOverlayView::contextMenuRequested, this,
            &ParamEditorGraphicsView::showAnchorContextMenu);
}

void ParamEditorGraphicsView::setDataContext(SingingClip *clip) {
    if (m_editMode == ParamEditorEditMode::Anchor)
        m_anchorController.cancel();
    else if (m_foreground)
        m_foreground->discardAction();
    disarmEdgeAutoScroll();
    clip == nullptr ? moveToNullClipState() : moveToSingingClipState(clip);
}

SpeakerMixEditorView *ParamEditorGraphicsView::speakerMixView() const {
    return m_speakerMixView;
}

QColor ParamEditorGraphicsView::paramGraduateColor() const {
    return m_foreground->graduateColor();
}

void ParamEditorGraphicsView::setParamGraduateColor(const QColor &color) {
    m_foreground->setGraduateColor(color);
    m_background->setGraduateColor(color);
}

QColor ParamEditorGraphicsView::paramOriginalCurveColor() const {
    return m_foreground->originalCurveColor();
}

void ParamEditorGraphicsView::setParamOriginalCurveColor(const QColor &color) {
    m_foreground->setOriginalCurveColor(color);
    m_background->setOriginalCurveColor(color);
}

QColor ParamEditorGraphicsView::paramEditedCurveColor() const {
    return m_foreground->editedCurveColor();
}

void ParamEditorGraphicsView::setParamEditedCurveColor(const QColor &color) {
    m_foreground->setEditedCurveColor(color);
    m_background->setEditedCurveColor(color);
}

bool ParamEditorGraphicsView::paramUseTrackColorForEditedCurve() const {
    return m_foreground->useTrackColorForEditedCurve();
}

void ParamEditorGraphicsView::setParamUseTrackColorForEditedCurve(bool on) {
    m_foreground->setUseTrackColorForEditedCurve(on);
    m_background->setUseTrackColorForEditedCurve(on);
}

QColor ParamEditorGraphicsView::paramBackgroundLayerColor() const {
    return m_foreground->backgroundLayerColor();
}

void ParamEditorGraphicsView::setParamBackgroundLayerColor(const QColor &color) {
    m_foreground->setBackgroundLayerColor(color);
    m_background->setBackgroundLayerColor(color);
}

QColor ParamEditorGraphicsView::anchorColor() const {
    return m_anchorOverlay->anchorColor();
}

void ParamEditorGraphicsView::setAnchorColor(const QColor &color) {
    m_anchorOverlay->setAnchorColor(color);
}

QColor ParamEditorGraphicsView::anchorSelectedColor() const {
    return m_anchorOverlay->anchorSelectedColor();
}

void ParamEditorGraphicsView::setAnchorSelectedColor(const QColor &color) {
    m_anchorOverlay->setAnchorSelectedColor(color);
}

QColor ParamEditorGraphicsView::anchorCurveColor() const {
    return m_anchorOverlay->anchorCurveColor();
}

void ParamEditorGraphicsView::setAnchorCurveColor(const QColor &color) {
    m_anchorOverlay->setAnchorCurveColor(color);
}

QColor ParamEditorGraphicsView::anchorPreviewColor() const {
    return m_anchorOverlay->anchorPreviewColor();
}

void ParamEditorGraphicsView::setAnchorPreviewColor(const QColor &color) {
    m_anchorOverlay->setAnchorPreviewColor(color);
}

QColor ParamEditorGraphicsView::speakerMixTextColor() const {
    return m_speakerMixView->textColor();
}

void ParamEditorGraphicsView::setSpeakerMixTextColor(const QColor &color) {
    m_speakerMixView->setTextColor(color);
}

QColor ParamEditorGraphicsView::speakerMixKeyframeLineColor() const {
    return m_speakerMixView->keyframeLineColor();
}

void ParamEditorGraphicsView::setSpeakerMixKeyframeLineColor(const QColor &color) {
    m_speakerMixView->setKeyframeLineColor(color);
}

QColor ParamEditorGraphicsView::speakerMixSelectedDotColor() const {
    return m_speakerMixView->selectedDotColor();
}

void ParamEditorGraphicsView::setSpeakerMixSelectedDotColor(const QColor &color) {
    m_speakerMixView->setSelectedDotColor(color);
}

QColor ParamEditorGraphicsView::speakerMixSelectionBorderColor() const {
    return m_speakerMixView->selectionBorderColor();
}

void ParamEditorGraphicsView::setSpeakerMixSelectionBorderColor(const QColor &color) {
    m_speakerMixView->setSelectionBorderColor(color);
}

QColor ParamEditorGraphicsView::speakerMixSelectionFillColor() const {
    return m_speakerMixView->selectionFillColor();
}

void ParamEditorGraphicsView::setSpeakerMixSelectionFillColor(const QColor &color) {
    m_speakerMixView->setSelectionFillColor(color);
}

void ParamEditorGraphicsView::discardAction() {
    if (m_speakerMixMode)
        return;
    if (m_editMode == ParamEditorEditMode::Anchor) {
        m_anchorController.cancel();
        disarmEdgeAutoScroll();
        return;
    }
    if (appStatus->currentEditObject != AppStatus::EditObjectType::Param)
        return;
    if (m_foreground)
        m_foreground->discardAction();
}

void ParamEditorGraphicsView::commitAction() {
    if (m_speakerMixMode || appStatus->currentEditObject != AppStatus::EditObjectType::Param)
        return;
    if (m_editMode == ParamEditorEditMode::Anchor)
        return;
    if (m_foreground)
        m_foreground->commitAction();
}

void ParamEditorGraphicsView::setEditMode(const ParamEditorEditMode mode) {
    if (m_editMode == mode)
        return;
    if (m_editMode == ParamEditorEditMode::Anchor)
        m_anchorController.setEditActive(false);
    else
        m_foreground->discardAction();
    disarmEdgeAutoScroll();

    m_editMode = mode;
    const bool anchorActive = mode == ParamEditorEditMode::Anchor && !m_speakerMixMode;
    m_foreground->setEraseMode(mode == ParamEditorEditMode::Erase);
    m_foreground->setBakeMode(mode == ParamEditorEditMode::Bake);
    m_anchorOverlay->setInteractive(anchorActive);
    m_anchorController.setEditActive(anchorActive);
}

ParamEditorEditMode ParamEditorGraphicsView::editMode() const {
    return m_editMode;
}

void ParamEditorGraphicsView::setForegroundBaseCurveVisible(const bool visible) {
    m_foreground->setBaseCurveVisible(visible);
}

void ParamEditorGraphicsView::setBackgroundBaseCurveVisible(const bool visible) {
    m_background->setBaseCurveVisible(visible);
}

void ParamEditorGraphicsView::setForeground(const ParamInfo::Name name,
                                            const ParamProperties &properties) {
    if (m_editMode == ParamEditorEditMode::Anchor)
        m_anchorController.cancel();
    else
        m_foreground->discardAction();
    disarmEdgeAutoScroll();
    if (name == ParamInfo::SpeakerMix) {
        if (m_editMode == ParamEditorEditMode::Anchor)
            m_anchorController.setEditActive(false);
        m_anchorOverlay->setInteractive(false);
        m_anchorOverlay->setAnchorsVisible(false);
        m_anchorController.setAlwaysVisible(false);
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
    if (!m_clip) {
        m_foreground->clearParams();
        m_anchorController.loadFromModel({});
        m_anchorOverlay->setAnchorsVisible(false);
        m_anchorController.setAlwaysVisible(false);
        return;
    }
    updateForeground(Param::Original, *m_clip->params.getParamByName(m_foregroundParam));
    updateForeground(Param::Edited, *m_clip->params.getParamByName(m_foregroundParam));
    m_anchorController.setAlwaysVisible(true);
    m_anchorOverlay->setAnchorsVisible(true);
    if (m_editMode == ParamEditorEditMode::Anchor) {
        m_anchorOverlay->setInteractive(true);
        m_anchorController.setEditActive(true);
    }
}

void ParamEditorGraphicsView::setBackground(const ParamInfo::Name name,
                                            const ParamProperties &properties) {
    m_backgroundParam = name;
    m_background->setParamProperties(properties);
    if (!m_clip) {
        m_background->clearParams();
        return;
    }
    updateBackground(Param::Original, *m_clip->params.getParamByName(m_backgroundParam));
    updateBackground(Param::Edited, *m_clip->params.getParamByName(m_backgroundParam));
}

void ParamEditorGraphicsView::updateForeground(const Param::Type type, const Param &param) {
    if (type == Param::Original) {
        m_foreground->loadOriginal(
            getDrawCurves(param.curves(m_debugMode ? Param::Edited : Param::Original)));
    } else if (type == Param::Edited) {
        m_foreground->loadEdited(getDrawCurves(param.curves(Param::Edited)));
        m_anchorController.loadFromModel(getAnchorCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::updateBackground(const Param::Type type, const Param &param) const {
    if (type == Param::Original) {
        m_background->loadOriginal(getDrawCurves(param.curves(Param::Original)));
    } else if (type == Param::Edited) {
        m_background->loadEdited(getDrawCurves(param.curves(Param::Edited)));
        m_background->loadAnchorEdited(getAnchorCurves(param.curves(Param::Edited)));
    } else if (type == Param::Envelope) {
        // TODO: handle envelope param
    }
}

void ParamEditorGraphicsView::onClipPropertyChanged() {
    setSceneLength(m_clip->length());
    setOffset(m_clip->start());
}

void ParamEditorGraphicsView::onParamChanged(const ParamInfo::Name name, const Param::Type type) {
    const auto param = m_clip->params.getParamByName(name);
    if (m_foregroundParam == name)
        updateForeground(type, *param);
    if (m_backgroundParam == name)
        updateBackground(type, *param);
}

void ParamEditorGraphicsView::onSpeakerMixChanged() const {
    updateSpeakerMixViewData();
}

void ParamEditorGraphicsView::onEditCompleted(const QList<DrawCurve *> &curves) {
    if (!m_clip)
        return;
    const auto *param = m_clip->params.getParamByName(m_foregroundParam);
    auto list = AnchorEditor::replaceDrawCurves(param->curves(Param::Edited), curves);
    clipController->onParamEdited(m_foregroundParam, list);
    qDeleteAll(list);
}

void ParamEditorGraphicsView::onEditStarted() {
    if (!m_clip || m_speakerMixMode)
        return;
    if (m_drawSessionId != 0 || editSessionManager->hasActiveTransaction())
        return;
    m_drawSessionId = editSessionManager->beginTransaction(
        AppStatus::EditObjectType::Param, m_clip->id(), {}, {}, {}, {m_foregroundParam});
    if (m_drawSessionId != 0)
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
}

void ParamEditorGraphicsView::onEditCommitted() {
    const auto sessionId = m_drawSessionId;
    m_drawSessionId = 0;
    if (sessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == sessionId) {
        editSessionManager->endTransaction(sessionId, EditSessionEndReason::Commit);
    }
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void ParamEditorGraphicsView::onEditDiscarded() {
    const auto sessionId = m_drawSessionId;
    m_drawSessionId = 0;
    if (sessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == sessionId) {
        editSessionManager->endTransaction(sessionId, EditSessionEndReason::Discard);
    }
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

bool ParamEditorGraphicsView::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (m_editMode == ParamEditorEditMode::Anchor && !m_speakerMixMode &&
            AnchorEditor::AnchorEditController::handlesKey(key)) {
            if (event->type() == QEvent::KeyPress) {
                m_anchorController.handleKeyPress(key);
                if (key == Qt::Key_Escape)
                    disarmEdgeAutoScroll();
            }
            event->accept();
            return true;
        } else if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        if (m_editMode == ParamEditorEditMode::Anchor) {
            m_anchorController.cancel();
            disarmEdgeAutoScroll();
        } else {
            discardAction();
        }
    }
    return TimeGraphicsView::event(event);
}

void ParamEditorGraphicsView::deleteSelection() {
    if (m_speakerMixMode) {
        m_speakerMixView->deleteSelection();
    } else if (m_editMode == ParamEditorEditMode::Anchor) {
        m_anchorController.deleteSelectedNodes();
    }
}

void ParamEditorGraphicsView::onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                                    const Qt::KeyboardModifiers modifiers) {
    if (m_editMode == ParamEditorEditMode::Anchor && m_anchorController.edgeAutoScrollAxes()) {
        m_anchorController.continueDragAtScene(mapToScene(clampedViewportPos));
        return;
    }
    TimeGraphicsView::onEdgeAutoScrollFrame(clampedViewportPos, modifiers);
}

void ParamEditorGraphicsView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        emit wheelHorScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        emit wheelHorScroll(event);
    }
}

void ParamEditorGraphicsView::moveToNullClipState() {
    m_anchorController.setEditActive(false);
    m_anchorController.setAlwaysVisible(false);
    m_anchorController.loadFromModel({});
    m_anchorOverlay->setInteractive(false);
    m_anchorOverlay->setAnchorsVisible(false);
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
    const bool anchorActive = m_editMode == ParamEditorEditMode::Anchor && !m_speakerMixMode;
    m_anchorController.setAlwaysVisible(!m_speakerMixMode);
    m_anchorOverlay->setAnchorsVisible(!m_speakerMixMode);
    m_anchorOverlay->setInteractive(anchorActive);
    m_anchorController.setEditActive(anchorActive);

    connect(clip, &SingingClip::propertyChanged, this,
            &ParamEditorGraphicsView::onClipPropertyChanged);
    connect(clip, &SingingClip::paramChanged, this, &ParamEditorGraphicsView::onParamChanged);
    connect(clip, &SingingClip::voiceContextChanged, this,
            [this](const VoiceContextChange &) { onSpeakerMixChanged(); });
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

QList<AnchorCurve *> ParamEditorGraphicsView::getAnchorCurves(const QList<Curve *> &curves) {
    QList<AnchorCurve *> result;
    for (auto *curve : curves) {
        if (curve->type() == Curve::Anchor)
            MathUtils::binaryInsert(result, static_cast<AnchorCurve *>(curve));
    }
    return result;
}

bool ParamEditorGraphicsView::beginAnchorEditSession() {
    if (!m_clip || m_speakerMixMode)
        return false;
    if (m_anchorSessionId != 0)
        return true;
    if (editSessionManager->hasActiveTransaction())
        return false;
    m_anchorSessionId = editSessionManager->beginTransaction(
        AppStatus::EditObjectType::Param, m_clip->id(), {}, {}, {}, {m_foregroundParam});
    if (m_anchorSessionId != 0)
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    return m_anchorSessionId != 0;
}

void ParamEditorGraphicsView::publishAnchors(const QList<AnchorCurve *> &curves) {
    if (!m_clip || m_speakerMixMode)
        return;
    const auto *param = m_clip->params.getParamByName(m_foregroundParam);
    auto edited = AnchorEditor::replaceAnchors(param->curves(Param::Edited), curves);
    clipController->onParamEdited(m_foregroundParam, edited);
    qDeleteAll(edited);
}

void ParamEditorGraphicsView::finishAnchorEditSession(const AnchorEditor::EditFinishReason reason) {
    const auto sessionId = m_anchorSessionId;
    m_anchorSessionId = 0;
    if (sessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == sessionId) {
        editSessionManager->endTransaction(sessionId,
                                           reason == AnchorEditor::EditFinishReason::Commit
                                               ? EditSessionEndReason::Commit
                                               : EditSessionEndReason::Discard);
    }
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void ParamEditorGraphicsView::onAnchorStateChanged() {
    if (m_renderedAnchorRevision != m_anchorController.curveRevision()) {
        m_renderedAnchorRevision = m_anchorController.curveRevision();
        m_foreground->loadAnchorEdited(m_anchorController.curves());
    }
    m_anchorOverlay->update();
}

void ParamEditorGraphicsView::showAnchorContextMenu(const QPointF scenePos,
                                                    const QPoint screenPos) {
    AnchorEditor::MenuInfo info;
    if (!m_anchorController.prepareMenu(scenePos, info))
        return;

    Menu menu(this);
    auto *linear = menu.addAction(tr("Linear"));
    linear->setCheckable(true);
    linear->setChecked(!info.mixedInterpolation && info.interpolation == AnchorNode::Linear);
    linear->setEnabled(info.interpolationEnabled);
    connect(linear, &QAction::triggered, this,
            [this] { m_anchorController.setSelectedInterpolation(AnchorNode::Linear); });

    auto *hermite = menu.addAction(tr("Hermite"));
    hermite->setCheckable(true);
    hermite->setChecked(!info.mixedInterpolation && info.interpolation == AnchorNode::Hermite);
    hermite->setEnabled(info.interpolationEnabled);
    connect(hermite, &QAction::triggered, this,
            [this] { m_anchorController.setSelectedInterpolation(AnchorNode::Hermite); });

    auto *interpolationGroup = new QActionGroup(&menu);
    interpolationGroup->setExclusive(true);
    interpolationGroup->addAction(linear);
    interpolationGroup->addAction(hermite);

    menu.addSeparator();
    auto *remove = menu.addAction(tr("&Delete"));
    remove->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    connect(remove, &QAction::triggered, this,
            [this] { m_anchorController.deleteSelectedNodes(); });
    menu.exec(screenPos);
}
