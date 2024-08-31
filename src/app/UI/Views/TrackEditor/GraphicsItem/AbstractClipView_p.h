//
// Created by fluty on 24-7-25.
//

#ifndef ABSTRACTCLIPGRAPHICSITEM_P_H
#define ABSTRACTCLIPGRAPHICSITEM_P_H

#include "UI/Views/Common/CommonGraphicsRectItem.h"

#include <QString>

class Menu;
class QWidget;
class AbstractClipView;

class AbstractClipViewPrivate {
    Q_DECLARE_PUBLIC(AbstractClipView)
public:
    explicit AbstractClipViewPrivate(AbstractClipView *q) : q_ptr(q){};

    QString m_name;
    int m_start = 0;
    int m_length = 0;
    int m_clipStart = 0;
    int m_clipLen = 0;
    double m_gain = 0;
    // double m_pan = 0;
    bool m_mute = false;
    bool m_activeClip = false;
    bool m_canResizeLength = false;
    QPointF m_mouseDownPos;
    int m_mouseDownStart = 0;
    int m_mouseDownClipStart = 0;
    int m_mouseDownLength = 0;
    int m_mouseDownClipLen = 0;
    bool m_propertyEdited = false;
    bool m_tempQuantizeOff = false;

    int m_trackIndex = 0;
    int m_quantize = 16;
    bool m_showDebugInfo = false;

    [[nodiscard]] QRectF previewRect() const;

private:
    AbstractClipView *q_ptr;
};


#endif // ABSTRACTCLIPGRAPHICSITEM_P_H
