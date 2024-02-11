//
// Created by fluty on 2024/1/22.
//

#ifndef SINGINGCLIPGRAPHICSITEM_H
#define SINGINGCLIPGRAPHICSITEM_H

#include "AbstractClipGraphicsItem.h"
#include "Model/Note.h"
#include "Utils/OverlapableSerialList.h"

class SingingClipGraphicsItem final : public AbstractClipGraphicsItem {
public:
    class NoteViewModel {
    public:
        int rStart;
        int length;
        int keyIndex;
    };

    explicit SingingClipGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    ~SingingClipGraphicsItem() override = default;

    void loadNotes(const OverlapableSerialList<Note> &notes);
    QString audioCachePath() const;
    void setAudioCachePath(const QString &path);

private:
    // void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) override;
    void addMenuActions(QMenu *menu) override;
    QString clipTypeName() override {
        return "[Singing] ";
    }

    QString m_audioCachePath;
    QList<NoteViewModel> m_notes;
};



#endif //SINGINGCLIPGRAPHICSITEM_H
