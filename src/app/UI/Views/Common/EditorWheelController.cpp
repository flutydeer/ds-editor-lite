#include "EditorWheelController.h"

#include "EditorViewportController.h"

#include "Global/AppGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <QNativeGestureEvent>
#include <QWidget>
#include <QWheelEvent>

bool isEditorWheelAnimationEnabled() {
#if defined(WITH_DIRECT_MANIPULATION)
    return !appOptions->appearance()->enableDirectManipulation;
#else
    return true;
#endif
}

EditorWheelController::EditorWheelController(EditorViewportController *viewport, QWidget *widget)
    : m_viewport(viewport), m_widget(widget) {
    m_input.setDiscreteAnimationEnabled(isEditorWheelAnimationEnabled);

    const auto installScrollTarget = [this](const Qt::Orientation orientation,
                                            const double viewportFraction) {
        m_input.setScrollTarget(
            orientation,
            {
                .value =
                    [this, orientation] {
                        return orientation == Qt::Horizontal ? m_viewport->horizontalOffset()
                                                             : m_viewport->verticalOffset();
                    },
                .setValue =
                    [this, orientation](const double value) {
                        auto offset = m_viewport->offset();
                        if (orientation == Qt::Horizontal)
                            offset.setX(value);
                        else
                            offset.setY(value);
                        m_viewport->setOffset(offset);
                    },
                .boundedValue =
                    [this, orientation](const double value) {
                        return static_cast<double>(qBound(
                            0, qRound(value), qRound(m_viewport->maximumOffset(orientation))));
                    },
                .step =
                    [this, orientation, viewportFraction] {
                        const auto size = m_viewport->viewportSize();
                        return (orientation == Qt::Horizontal ? size.width() : size.height()) *
                               viewportFraction;
                    },
                .canScroll = [] { return true; },
            });
    };
    installScrollTarget(Qt::Horizontal, 0.2);
    installScrollTarget(Qt::Vertical, 0.15);

    m_input.setZoomTarget(Qt::Horizontal,
                          {
                              .value = [this] { return m_viewport->horizontalScale(); },
                              .setValueAt =
                                  [this](const double value, const double anchor) {
                                      m_viewport->setScale(
                                          value, m_viewport->verticalScale(),
                                          {anchor, m_viewport->viewportSize().height() * 0.5});
                                  },
                              .boundedValue =
                                  [this](const double value) {
                                      return m_viewport->boundedScale(Qt::Horizontal, value);
                                  },
                              .step = 0.4,
                          });
    m_input.setZoomTarget(Qt::Vertical,
                          {
                              .value = [this] { return m_viewport->verticalScale(); },
                              .setValueAt =
                                  [this](const double value, const double anchor) {
                                      m_viewport->setScale(
                                          m_viewport->horizontalScale(), value,
                                          {m_viewport->viewportSize().width() * 0.5, anchor});
                                  },
                              .boundedValue =
                                  [this](const double value) {
                                      return m_viewport->boundedScale(Qt::Vertical, value);
                                  },
                              .step = 0.3,
                          });
}

bool EditorWheelController::handleWheel(QWheelEvent *event) {
    return m_input.handleWheel(event);
}

bool EditorWheelController::handleNativeGesture(QNativeGestureEvent *event) {
    if (!event || !m_widget || event->gestureType() != Qt::ZoomNativeGesture)
        return false;
    const auto factor = event->value() + 1.0;
    if (factor <= 0.0)
        return true;
    const auto anchor = m_widget->mapFromGlobal(event->globalPosition().toPoint()).x();
    return m_input.zoomByFactor(Qt::Horizontal, factor, anchor);
}

bool EditorWheelController::horizontalScale(QWheelEvent *event) {
    return m_input.handleWheel(event, WheelInputController::Action::HorizontalZoom, Qt::Vertical);
}

bool EditorWheelController::verticalScale(QWheelEvent *event) {
    return m_input.handleWheel(event, WheelInputController::Action::VerticalZoom, Qt::Vertical);
}

bool EditorWheelController::horizontalScroll(QWheelEvent *event) {
    const auto sourceAxis = event->modifiers() == Qt::ShiftModifier ? Qt::Vertical : Qt::Horizontal;
    return m_input.handleWheel(event, WheelInputController::Action::HorizontalScroll, sourceAxis);
}

bool EditorWheelController::verticalScroll(QWheelEvent *event) {
    return m_input.handleWheel(event, WheelInputController::Action::VerticalScroll, Qt::Vertical);
}

void EditorWheelController::stop() {
    m_input.stop();
}
