#ifndef PIANOROLLCONTEXTMENUCONTROLLER_H
#define PIANOROLLCONTEXTMENUCONTROLLER_H

#include <QObject>
#include <QList>
#include <QPoint>
#include <QString>
#include <QVector>

class SingingClip;
class QWidget;
class Menu;

enum class PianoRollAnchorMode { None, Linear, Hermite, Mixed };

struct PianoRollMenuContext {
    enum class Target { Background, Note, Anchor };

    Target target = Target::Background;
    QPoint globalPos;
    int globalTick = 0;
    int keyIndex = -1;
    int noteId = -1;
    QList<int> selectedNoteIds;
    QString noteLanguage;
    bool phonemeEditorEnabled = false;
    bool pronunciationTarget = false;
    PianoRollAnchorMode anchorMode = PianoRollAnchorMode::None;
    bool anchorInterpolationEnabled = false;
};

struct PianoRollPastePreviewNote {
    int relativeStart = 0;
    int length = 0;
    int key = 60;
    QString lyric;
    QString pronunciation;
    bool pronunciationEdited = false;
    bool overlapped = false;
};

struct PianoRollPastePreviewData {
    QVector<PianoRollPastePreviewNote> notes;
};

class IPianoRollPastePreviewHost {
public:
    virtual ~IPianoRollPastePreviewHost() = default;
    virtual void showPianoRollPastePreview(const PianoRollPastePreviewData &data,
                                           int globalTick) = 0;
    virtual void clearPianoRollPastePreview() = 0;
};

class IAnchorCommandHost {
public:
    virtual ~IAnchorCommandHost() = default;
    virtual void setSelectedAnchorInterpolation(PianoRollAnchorMode mode) = 0;
    virtual void deleteSelectedAnchors() = 0;
};

class PianoRollContextMenuController final : public QObject {
public:
    explicit PianoRollContextMenuController(QWidget *owner);

    void showMenu(const PianoRollMenuContext &context, SingingClip *clip,
                  IPianoRollPastePreviewHost *previewHost, IAnchorCommandHost *anchorHost);
    void cutSelection() const;
    void copySelection() const;
    void pasteSelection() const;
    void deleteSelection(const QList<int> &noteIds = {}) const;
    void selectAll() const;

private:
    bool appendPronunciationCandidateActions(Menu &menu, SingingClip *clip, int noteId) const;
    void showPronunciationOnlyMenu(const PianoRollMenuContext &context, SingingClip *clip) const;
    void openPhonemeEditor(SingingClip *clip, int noteId) const;

    QWidget *m_owner = nullptr;
};

#endif // PIANOROLLCONTEXTMENUCONTROLLER_H
