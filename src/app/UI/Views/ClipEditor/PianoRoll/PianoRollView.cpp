//
// Created by fluty on 24-8-21.
//

#include "PianoRollView.h"

#include "IPianoRollCanvas.h"
#include "PianoKeyboardView.h"
#include "PhonemeView.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Views/EditorCanvas/EditorCanvasFactory.h"

#include <QEvent>
#include <QLabel>
#include <QLoggingCategory>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcPianoRollCanvas, "ds.editor.canvas.piano")

PianoRollView::PianoRollView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setMinimumHeight(128);

    m_canvas = EditorCanvasFactory::createPianoRollCanvas(this);

    m_timelineView = new TimelineView;
    m_timelineView->setObjectName("pianoRollTimelineView");
    m_timelineView->setTimeRange(m_canvas->startTick(), m_canvas->endTick());
    m_timelineView->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    m_timelineView->setQuantize(appStatus->pianoRollQuantize);
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, m_timelineView,
            &TimelineView::setQuantize);
    m_timelineView->setFixedHeight(ClipEditorGlobal::timelineViewHeight);

    m_keyboardView = new PianoKeyboardView;
    const auto viewport = m_canvas->viewportState();
    m_keyboardView->setKeyRange(viewport.topValue, viewport.bottomValue);

    m_phonemeView = new PhonemeView;
    m_phonemeView->setTimeRange(m_canvas->startTick(), m_canvas->endTick());
    m_phonemeView->setFixedHeight(40);
    m_phonemeView->setVisible(false);
    connect(m_phonemeView, &PhonemeView::wheelHorScale, this, &PianoRollView::onWheelHorScale);
    connect(m_phonemeView, &PhonemeView::wheelHorScroll, this, &PianoRollView::onWheelHorScroll);

    m_lbTip = new QLabel(tr("Select a singing clip to edit"));
    m_lbTip->setObjectName("lbNullClipTip");
    m_lbTip->setAlignment(Qt::AlignCenter);

    const auto topLeftSpacing = new QWidget();
    topLeftSpacing->setObjectName("pianoRollTopLeftSpacing");
    topLeftSpacing->setMinimumWidth(0);
    topLeftSpacing->setFixedHeight(ClipEditorGlobal::timelineViewHeight);

    const auto bottomLeftSpacing = new QWidget();
    bottomLeftSpacing->setObjectName("pianoRollBottomLeftSpacing");
    bottomLeftSpacing->setMinimumWidth(0);
    bottomLeftSpacing->setFixedHeight(m_phonemeView->height());

    const auto pianoKeyboardLayout = new QVBoxLayout;
    pianoKeyboardLayout->setContentsMargins(0, 0, 0, 0);
    pianoKeyboardLayout->setSpacing(0);
    pianoKeyboardLayout->addWidget(topLeftSpacing);
    pianoKeyboardLayout->addWidget(m_keyboardView);
    pianoKeyboardLayout->addWidget(bottomLeftSpacing);

    m_rightLayout = new QVBoxLayout;
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(0);
    m_rightLayout->addWidget(m_timelineView);
    m_rightLayout->addWidget(m_canvas->widget());
    m_rightLayout->addWidget(m_phonemeView);

    const auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(pianoKeyboardLayout);
    layout->addLayout(m_rightLayout, 1);
    layout->addWidget(m_lbTip);
    setLayout(layout);

    connect(m_timelineView, &TimelineView::wheelHorScale, this, &PianoRollView::onWheelHorScale);
    connect(m_keyboardView, &PianoKeyboardView::wheelScroll, this,
            [this](QWheelEvent *event) { m_canvas->onWheelVerScale(event); });
    connectCanvas();
}

IPianoRollCanvas *PianoRollView::canvas() const {
    return m_canvas;
}

void PianoRollView::setDataContext(SingingClip *clip) {
    m_clip = clip;
    m_canvas->setDataContext(clip);
    m_phonemeView->setDataContext(clip);
    m_timelineView->setDataContext(clip);

    const bool notNull = clip != nullptr;
    m_timelineView->setVisible(notNull);
    m_canvas->widget()->setVisible(notNull);
    m_phonemeView->setVisible(notNull);
    m_keyboardView->setVisible(notNull);
    m_lbTip->setVisible(!notNull);
}

void PianoRollView::onEditModeChanged(const ClipEditorGlobal::PianoRollEditMode mode) {
    m_editMode = mode;
    m_canvas->setEditMode(mode);
}

void PianoRollView::setTrackColorIndex(const int index) {
    m_trackColorIndex = index;
    m_canvas->setTrackColorIndex(index);
    m_keyboardView->setTrackColorIndex(index);
}

void PianoRollView::setPlaybackPosition(const double tick) const {
    m_canvas->setPlaybackPosition(tick);
}

void PianoRollView::setLastPlaybackPosition(const double tick) const {
    m_canvas->setLastPlaybackPosition(tick);
}

void PianoRollView::onWheelHorScale(QWheelEvent *event) const {
    m_canvas->onWheelHorScale(event);
}

void PianoRollView::onWheelHorScroll(QWheelEvent *event) const {
    m_canvas->onWheelHorScroll(event);
}

void PianoRollView::connectCanvas() {
    connect(m_canvas, &IPianoRollCanvas::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_canvas, &IPianoRollCanvas::timeRangeChanged, m_phonemeView,
            &PhonemeView::setTimeRange);
    connect(m_canvas, &IPianoRollCanvas::keyRangeChanged, m_keyboardView,
            &PianoKeyboardView::setKeyRange);
    connect(m_canvas, &IPianoRollCanvas::keyHovered, m_keyboardView,
            &PianoKeyboardView::setHoveredKeyIndex);
    connect(m_canvas, &IPianoRollCanvas::keyHoverCleared, m_keyboardView,
            [this] { m_keyboardView->setHoveredKeyIndex(-1); });
    connect(m_canvas, &IPianoRollCanvas::scaleChanged, this, &PianoRollView::canvasScaleChanged);
    connect(m_canvas->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &PianoRollView::horizontalScrollValueChanged);
    connect(m_canvas, &IPianoRollCanvas::rendererFailed, this,
            &PianoRollView::scheduleLegacyFallback);
}

void PianoRollView::scheduleLegacyFallback(const QString &reason) {
    if (m_canvas->backend() != EditorCanvasBackend::ExperimentalRhi || m_fallbackPending)
        return;
    m_fallbackPending = true;
    qCWarning(lcPianoRollCanvas) << "RHI canvas failed; falling back to Legacy:" << reason;
    QTimer::singleShot(0, this, &PianoRollView::replaceCanvasWithLegacy);
}

void PianoRollView::replaceCanvasWithLegacy() {
    const auto viewport = m_canvas->viewportState();
    auto *failedCanvas = m_canvas;
    auto *failedWidget = failedCanvas->widget();
    m_rightLayout->removeWidget(failedWidget);
    disconnect(failedCanvas, nullptr, this, nullptr);
    disconnect(failedCanvas->horizontalScrollBar(), nullptr, this, nullptr);
    delete failedCanvas;
    delete failedWidget;

    m_canvas = EditorCanvasFactory::createPianoRollCanvas(EditorCanvasBackend::Legacy, this);
    m_rightLayout->insertWidget(1, m_canvas->widget());
    connectCanvas();
    m_canvas->setDataContext(m_clip);
    m_canvas->setEditMode(m_editMode);
    m_canvas->setTrackColorIndex(m_trackColorIndex);
    m_canvas->restoreViewportState(viewport);
    m_canvas->widget()->setVisible(!m_clip.isNull());
    m_fallbackPending = false;
}

void PianoRollView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_lbTip->setText(tr("Select a singing clip to edit"));
}
