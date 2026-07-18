//
// Created by fluty on 24-10-27.
//

#ifndef PRONUNCIATIONVIEW_H
#define PRONUNCIATIONVIEW_H

#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include "Utils/UniqueObject.h"

#include <QColor>

class PronunciationView final : public AbstractGraphicsRectItem, public UniqueObject {
    Q_OBJECT

public:
    explicit PronunciationView(int noteId, QGraphicsItem *parent = nullptr);
    ~PronunciationView() override;

    const int textHeight = 20;

    static QColor textColor();
    static void setTextColor(const QColor &color);

    void setEditingPronunciation(bool editing);
    [[nodiscard]] bool isEditingPronunciation() const;

private:
    friend class NoteView;
    void setPronunciation(const QString &pronunciation, bool edited);
    void setTextVisible(bool visible);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

    QString m_pronunciation;
    bool m_pronunciationEdited = false;
    bool m_textVisible = true;
    QPointF m_pos;
    QSizeF m_size;

    bool m_editingPronunciation = false;

    static QColor s_textColor;
};



#endif // PRONUNCIATIONVIEW_H
