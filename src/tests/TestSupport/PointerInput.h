#ifndef TESTSUPPORT_POINTERINPUT_H
#define TESTSUPPORT_POINTERINPUT_H

#include <QCursor>
#include <QWidget>
#include <QWindow>
#include <QtTest/QTest>

namespace TestSupport {
    // Route widget-local touch points through their window even when another window covers them.
    class WidgetTouchSequence {
    public:
        WidgetTouchSequence(QWidget &widget, QPointingDevice *device)
            : m_widget(widget),
              m_sequence(QTest::touchEvent(widget.window()->windowHandle(), device, false)) {
            Q_ASSERT(widget.window()->windowHandle());
        }

        WidgetTouchSequence &press(int id, const QPoint &position) {
            m_sequence.press(id, windowPosition(position));
            return *this;
        }

        WidgetTouchSequence &move(int id, const QPoint &position) {
            m_sequence.move(id, windowPosition(position));
            return *this;
        }

        WidgetTouchSequence &release(int id, const QPoint &position) {
            m_sequence.release(id, windowPosition(position));
            return *this;
        }

        WidgetTouchSequence &stationary(int id) {
            m_sequence.stationary(id);
            return *this;
        }

        bool commit(bool processEvents = true) {
            return m_sequence.commit(processEvents);
        }

    private:
        QPoint windowPosition(const QPoint &position) const {
            return m_widget.mapTo(m_widget.window(), position);
        }

        QWidget &m_widget;
        QTest::QTouchEventSequence m_sequence;
    };

    inline void hoverWidget(QWidget &surface, const QPoint &position) {
        const auto global = surface.mapToGlobal(position);
        // Cursor warping alone does not always produce a platform mouse move.
        QCursor::setPos(global);
        QCoreApplication::processEvents();
        auto *window = surface.window();
        QTest::mouseMove(window->windowHandle(), window->mapFromGlobal(global));
    }

    inline void clickWidget(QWidget &surface, const QPoint &position) {
        hoverWidget(surface, position);
        auto *window = surface.window();
        QTest::mouseClick(window->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                          window->mapFromGlobal(surface.mapToGlobal(position)));
    }
}

#endif
