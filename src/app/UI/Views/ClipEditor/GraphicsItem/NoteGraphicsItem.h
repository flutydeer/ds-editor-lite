//
// Created by fluty on 2024/1/23.
//

#ifndef NOTEGRAPHICSITEM_H
#define NOTEGRAPHICSITEM_H

#include "UI/Views/Common/CommonGraphicsRectItem.h"
#include "Utils/IOverlapable.h"
#include "Utils/UniqueObject.h"

class CMenu;

class NoteGraphicsItem final : public CommonGraphicsRectItem,
                               public UniqueObject,
                               public IOverlapable {
    Q_OBJECT

public:
    explicit NoteGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    explicit NoteGraphicsItem(int itemId, int start, int length, int keyIndex, const QString &lyric,
                              const QString &pronunciation, QGraphicsItem *parent = nullptr);

    [[nodiscard]] QWidget *context() const;
    void setContext(QWidget *context);

    [[nodiscard]] int start() const;
    void setStart(int start);
    [[nodiscard]] int length() const;
    void setLength(int length);
    [[nodiscard]] int keyIndex() const;
    void setKeyIndex(int keyIndex);
    [[nodiscard]] QString lyric() const;
    void setLyric(const QString &lyric);
    [[nodiscard]] QString pronunciation() const;
    void setPronunciation(const QString &pronunciation, bool edited);
    [[nodiscard]] bool editingPitch() const;
    void setEditingPitch(bool on);

    [[nodiscard]] int pronunciationTextHeight() const;

    // for handle move and resize
    [[nodiscard]] int startOffset() const;
    void setStartOffset(int tick);
    [[nodiscard]] int lengthOffset() const;
    void setLengthOffset(int tick);
    [[nodiscard]] int keyOffset() const;
    void setKeyOffset(int key);
    void resetOffset();

signals:
    void editLyricTriggered(int id);
    void removeTriggered(int id);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    // void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    void initUi();
    // void addMenuActions(QMenu *menu);

    QWidget *m_context{};
    CMenu *m_menu{};
    int m_start = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_pronunciation;
    bool m_pronunciationEdited = false;
    bool m_editingPitch = false;

    int m_startOffset = 0;
    int m_lengthOffset = 0;
    int m_keyOffset = 0;

    int m_pronunciationTextHeight = 20;
};

#endif // NOTEGRAPHICSITEM_H
