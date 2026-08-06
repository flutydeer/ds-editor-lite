#ifndef SINGINGCLIPGRAPHICSITEM_H
#define SINGINGCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ADT/OverlappableSerialList.h>

#include <QVector>

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
    void loadPreviewNotes(const QVector<std::tuple<int, int, int>> &notes);
    [[nodiscard]] int contentLength() const override;
    // 将场景坐标换算为钢琴卷帘落点的 keyIndex；不在音符绘制区内返回 -1
    [[nodiscard]] double keyIndexAtScenePos(const QPointF &scenePos) const;

public slots:
    void onNoteListChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void onNotePropertyChanged(const Note *note);
    void setSingerName(const QString &singerName);
    void setSpeakerName(const QString &speakerName);
    void setDefaultLanguage(const QString &language);

private:
    struct NoteLayout {
        int lowestKeyIndex = 127;
        int highestKeyIndex = 0;
        double noteHeight = 0.0;
        // 音符内容区顶部锚点（previewRect.top() + 垂直居中偏移）
        double contentTop = 0.0;

        [[nodiscard]] bool valid() const {
            return noteHeight > 0;
        }
    };

    // override;
    [[nodiscard]] QString text() const override;
    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) override;
    void drawPianoRollOverlay(QPainter *painter, double noteHeight, int highestKeyIndex,
                              double contentTop);
    [[nodiscard]] NoteLayout computeNoteLayout(const QRectF &previewRect) const;
    [[nodiscard]] QString clipTypeName() const override;
    [[nodiscard]] QString iconPath() const override;

    // 音符缩略图最大高度（像素）；超过时压缩并垂直居中绘制
    static constexpr double maxNoteHeight = 8.0;

    QList<NoteViewModel *> m_notes;
    QString m_singerName;
    QString m_speakerName;
    QString m_language = "unknown";

    void addNote(const Note *note);
    void removeNote(int id);
    void dispose();
};



#endif // SINGINGCLIPGRAPHICSITEM_H
