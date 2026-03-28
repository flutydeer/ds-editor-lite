//
// Created by fluty on 24-10-27.
//

#ifndef PRONUNCIATIONVIEW_H
#define PRONUNCIATIONVIEW_H

#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include "Utils/UniqueObject.h"

class QLineEdit;
class QGraphicsProxyWidget;

class PronunciationView final : public AbstractGraphicsRectItem, public UniqueObject {
    Q_OBJECT

public:
    explicit PronunciationView(int noteId, QGraphicsItem *parent = nullptr);
    ~PronunciationView() override;

    const int textHeight = 20;

    void startEditingPronunciation();
    void finishEditingPronunciation();
    bool isEditingPronunciation() const;

signals:
    void pronunciationEditingFinished(const QString &pronunciation);

private:
    friend class NoteView;
    void setPronunciation(const QString &pronunciation, bool edited);
    void setTextVisible(bool visible);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;
    void updateLineEditGeometry();
    bool eventFilter(QObject *obj, QEvent *event) override;

    QString m_pronunciation;
    bool m_pronunciationEdited = false;
    bool m_textVisible = true;
    QPointF m_pos;
    QSizeF m_size;

    QGraphicsProxyWidget *m_lineEditProxy = nullptr;
    QLineEdit *m_lineEdit = nullptr;
    bool m_editingPronunciation = false;
};



#endif // PRONUNCIATIONVIEW_H
