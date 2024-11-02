//
// Created by fluty on 2024/1/22.
//

#ifndef SINGINGCLIPGRAPHICSITEM_H
#define SINGINGCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"

#include "Model/AppModel/SingingClip.h"
#include "Utils/OverlappableSerialList.h"

class Note;

class SingingClipView final : public AbstractClipView {
    Q_OBJECT
public:
    class NoteViewModel : public Overlappable {
    public:
        int id = -1;
        int rStart = 0;
        int length = 0;
        int keyIndex = 0;

        int compareTo(const NoteViewModel *obj) const;
        static bool isOverlappedWith(NoteViewModel *obj);
        [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;
    };

    [[nodiscard]] ClipType clipType() const override {
        return Singing;
    }

    explicit SingingClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~SingingClipView() override;

    void loadNotes(const OverlappableSerialList<Note> &notes);
    [[nodiscard]] int contentLength() const override;

public slots:
    void onNoteListChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void onNotePropertyChanged(Note *note);
    void setDefaultLanguage(const QString &language);

private:
    // void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    // override;
    [[nodiscard]] QString text() const override;
    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) override;
    [[nodiscard]] QString clipTypeName() const override;
    [[nodiscard]] QString iconPath() const override;

    QList<NoteViewModel *> m_notes;
    QString m_language = "unknown";

    void addNote(Note *note);
    void removeNote(int id);
    // void updateNote(Note *note);
    void dispose();
};



#endif // SINGINGCLIPGRAPHICSITEM_H
