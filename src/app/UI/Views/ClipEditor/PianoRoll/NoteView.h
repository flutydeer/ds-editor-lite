//
// Created by fluty on 2024/1/23.
//

#ifndef NOTEGRAPHICSITEM_H
#define NOTEGRAPHICSITEM_H

#include "Global/AppGlobal.h"
#include "UI/Utils/OverlappableItem.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include <lite/ADT/Property.h>
#include "Utils/UniqueObject.h"

#include <QColor>

class PronunciationView;

class NoteView final : public AbstractGraphicsRectItem,
                       public UniqueObject,
                       public OverlappableItem {
    Q_OBJECT

public:
    explicit NoteView(int itemId, QGraphicsItem *parent = nullptr);
    ~NoteView() override;

    static int trackColorIndex();
    static void setTrackColorIndex(int index);
    // NOTE: transitional; may move to AppColorPalette (per-index or contrast-derived)
    static QColor selectedBorderColor();
    static void setSelectedBorderColor(const QColor &color);
    [[nodiscard]] int rStart() const;
    void setRStart(int rStart);
    [[nodiscard]] int length() const;
    void setLength(int length);
    [[nodiscard]] int keyIndex() const;
    void setKeyIndex(int keyIndex);
    [[nodiscard]] QString lyric() const;
    void setLyric(const QString &lyric);
    void setPronunciation(const QString &pronunciation, bool edited);
    [[nodiscard]] bool editingPitch() const;
    void setEditingPitch(bool on);
    [[nodiscard]] PronunciationView *pronunciationView() const;
    void setPronunciationView(PronunciationView *view);

    [[nodiscard]] int startOffset() const;
    void setStartOffset(int tick);
    [[nodiscard]] int lengthOffset() const;
    void setLengthOffset(int tick);
    [[nodiscard]] int keyOffset() const;
    void setKeyOffset(int key);
    void resetOffset();

    // Style
    Property<int> fontPixelSize = 13;

    // Inline editing
    void setEditingLyric(bool editing);
    [[nodiscard]] bool isEditingLyric() const;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;
    void adjustPronView() const;
    void initUi();

    PronunciationView *m_pronView = nullptr;
    bool m_editingLyric = false;
    int m_rStart = 0;
    int m_length = AppGlobal::ticksPerQuarterNote;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_pronunciation;
    bool m_pronunciationEdited = false;
    bool m_editingPitch = false;

    int m_startOffset = 0;
    int m_lengthOffset = 0;
    int m_keyOffset = 0;

    static int s_trackColorIndex;
    static QColor s_selectedBorderColor;
};

#endif // NOTEGRAPHICSITEM_H
