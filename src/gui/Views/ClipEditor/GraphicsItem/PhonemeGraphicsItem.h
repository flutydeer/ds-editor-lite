//
// Created by fluty on 24-2-13.
//

#ifndef PHONEMEGRAPHICSITEM_H
#define PHONEMEGRAPHICSITEM_H

#include <QString>

#include "Views/ClipEditor/ClipEditorGlobal.h"
#include "Views/Common/CommonGraphicsRectItem.h"

using namespace ClipEditorGlobal;

class PhonemeGraphicsItem final : public CommonGraphicsRectItem {
    Q_OBJECT

public:
    enum PhonemeItemType { Ahead, Normal, Final, Sil };
    explicit PhonemeGraphicsItem(int noteId, PhonemeItemType type, QGraphicsItem *parent = nullptr);

    int start() const;
    void setStart(int start);
    int length() const;
    void setLength(int length);
    QString name() const;
    void setName(const QString &name);
    PhonemeItemType itemType() const;

    int startOffset() const;
    void setStartOffset(int tick);
    int lengthOffset() const;
    void setLengthOffset(int tick);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;

    int m_noteId;
    int m_start = 0;
    int m_length = 480;
    QString m_name;
    PhonemeItemType m_itemType;
    int m_startOffset = 0;
    int m_lengthOffset = 0;
};



#endif // PHONEMEGRAPHICSITEM_H
