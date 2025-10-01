//
// Created by fluty on 24-3-1.
//

#ifndef SCALABLEITEM_H
#define SCALABLEITEM_H

#include <QRectF>

#include "IScalable.h"

class IScalableItem : public IScalable {
public:
    [[nodiscard]] QRectF visibleRect() const;
    void setVisibleRect(const QRectF &rect);

protected:
    virtual void afterSetVisibleRect() = 0;

private:
    QRectF m_visibleRect;
};

inline QRectF IScalableItem::visibleRect() const {
    return m_visibleRect;
}

inline void IScalableItem::setVisibleRect(const QRectF &rect) {
    m_visibleRect = rect;
    afterSetVisibleRect();
}
#endif // SCALABLEITEM_H
