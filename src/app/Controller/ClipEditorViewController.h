//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEWCONTROLLER_H
#define CLIPEDITVIEWCONTROLLER_H

#define clipController ClipEditorViewController::instance()

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Utils/Singleton.h"

#include <QObject>

class ClipEditorViewControllerPrivate;
class IClipEditorView;

class ClipEditorViewController final : public QObject, public Singleton<ClipEditorViewController> {
    Q_OBJECT

public:
    explicit  ClipEditorViewController();
    ~ClipEditorViewController() override;
    void setView(IClipEditorView *view);
    void setClip(Clip *clip);
    void copySelectedNotesWithParams() const;
    void cutSelectedNotesWithParams();
    void pasteNotesWithParams(const NotesParamsInfo &info, int tick);

    [[nodiscard]] bool canSelectAll() const;
    [[nodiscard]] bool hasSelectedNotes() const;

    // View operations
    void centerAt(double tick, double keyIndex);
    void centerAt(const Note &note);

signals:
    // TODO: 连接到 clip 模型监听更改
    void canSelectAllChanged(bool canSelectAll);
    void hasSelectedNotesChanged(bool has);

public slots:
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    void onRemoveNotes(const QList<int> &notesId);
    void onInsertNote(Note *note);
    void onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey);
    void onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const;
    void onResizeNotesRight(const QList<int> &notesId, int deltaTick) const;
    void onAdjustPhoneme(const QList<int> &notesId, const QList<Phoneme> &phonemes) const;
    void onNoteSelectionChanged(const QList<int> &notesId, bool unselectOther);
    void onOriginalPitchChanged(const OverlappableSerialList<Curve> &curves) const;
    void onPitchEdited(const OverlappableSerialList<Curve> &curves) const;
    void onDeleteSelectedNotes();
    void onSelectAllNotes();
    void onFillLyric(QWidget *parent);

private:
    Q_DECLARE_PRIVATE(ClipEditorViewController);
    ClipEditorViewControllerPrivate *d_ptr;
};



#endif // CLIPEDITVIEWCONTROLLER_H
