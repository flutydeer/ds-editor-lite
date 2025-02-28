//
// Created by fluty on 2024/1/24.
//

#ifndef PIANOROLLBACKGROUNDGRAPHICSITEM_H
#define PIANOROLLBACKGROUNDGRAPHICSITEM_H

#include "UI/Views/Common/TimeGridView.h"

class PianoRollBackground final : public TimeGridView {
    Q_OBJECT

private:
    friend class PianoRollGraphicsView;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Properties
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &color);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &color);
    QColor octaveDividerColor() const;
    void setOctaveDividerColor(const QColor &color);

    QColor m_whiteKeyColor = {41, 44, 54};
    QColor m_blackKeyColor = {35, 38, 46};
    QColor m_octaveDividerColor = {28, 32, 36};
};



#endif // PIANOROLLBACKGROUNDGRAPHICSITEM_H
