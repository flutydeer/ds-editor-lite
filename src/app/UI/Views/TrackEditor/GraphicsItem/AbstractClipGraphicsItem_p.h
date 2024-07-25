//
// Created by fluty on 24-7-25.
//

#ifndef ABSTRACTCLIPGRAPHICSITEM_P_H
#define ABSTRACTCLIPGRAPHICSITEM_P_H

#include "UI/Views/Common/CommonGraphicsRectItem.h"

#include <QString>

class Menu;
class QWidget;
class AbstractClipGraphicsItem;
class AbstractClipGraphicsItemPrivate {
    Q_DECLARE_PUBLIC(AbstractClipGraphicsItem)
public:
    enum MouseMoveBehavior { Move, ResizeRight, ResizeLeft, None };

    explicit AbstractClipGraphicsItemPrivate(AbstractClipGraphicsItem *q) : q_ptr(q){};

    QString m_name;
    int m_start = 0;
    int m_length = 0;
    int m_clipStart = 0;
    int m_clipLen = 0;
    double m_gain = 0;
    // double m_pan = 0;
    bool m_mute = false;
    QRectF m_rect;
    int m_resizeTolerance = 8; // px
    bool m_canResizeLength = false;
    // bool m_mouseOnResizeRightArea = false;
    // bool m_mouseOnResizeLeftArea = false;

    MouseMoveBehavior m_mouseMoveBehavior = Move;
    QPointF m_mouseDownPos;
    // QPointF m_mouseDownScenePos;
    int m_mouseDownStart{};
    int m_mouseDownClipStart{};
    int m_mouseDownLength{};
    int m_mouseDownClipLen{};
    bool m_propertyEdited = false;

    int m_trackIndex = 0;
    int m_quantize = 16;
    bool m_tempQuantizeOff = false;
    bool m_showDebugInfo = false;

    [[nodiscard]] QRectF previewRect() const;

private:
    AbstractClipGraphicsItem *q_ptr;
};


#endif // ABSTRACTCLIPGRAPHICSITEM_P_H
