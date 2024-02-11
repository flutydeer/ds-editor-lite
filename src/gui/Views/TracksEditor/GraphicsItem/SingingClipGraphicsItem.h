//
// Created by fluty on 2024/1/22.
//

#ifndef SINGINGCLIPGRAPHICSITEM_H
#define SINGINGCLIPGRAPHICSITEM_H

#include "AbstractClipGraphicsItem.h"
#include "Model/Clip.h"
#include "Model/Note.h"
#include "Utils/OverlapableSerialList.h"

class SingingClipGraphicsItem final : public AbstractClipGraphicsItem {
    Q_OBJECT
public:
    class NoteViewModel {
    public:
        int id;
        int rStart;
        int length;
        int keyIndex;
    };

    explicit SingingClipGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    ~SingingClipGraphicsItem() override = default;

    void loadNotes(const OverlapableSerialList<Note> &notes);
    QString audioCachePath() const;
    void setAudioCachePath(const QString &path);

public slots:
    void onNoteChanged(SingingClip::NoteChangeType type, int id, Note *note);

private:
    // void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    // override;
    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) override;
    void addMenuActions(QMenu *menu) override;
    QString clipTypeName() override {
        return "[Singing] ";
    }

    QString m_audioCachePath;
    QList<NoteViewModel> m_notes;

    void addNote(Note *note);
    void removeNote(int id);
};



#endif // SINGINGCLIPGRAPHICSITEM_H
