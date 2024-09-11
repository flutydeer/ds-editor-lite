//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEWCONTROLLER_H
#define CLIPEDITVIEWCONTROLLER_H

#define clipController ClipEditorViewController::instance()

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Model/NoteDialogResult/NoteDialogResult.h"
#include "Utils/Singleton.h"

#include <QObject>

class ClipEditorViewControllerPrivate;
class IClipEditorView;

class ClipEditorViewController final : public QObject, public Singleton<ClipEditorViewController> {
    Q_OBJECT

public:
    explicit ClipEditorViewController();
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
    // TODO: 连接到 AppStatus 模型监听更改
    void canSelectAllChanged(bool canSelectAll);
    void hasSelectedNotesChanged(bool has);

public slots:
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    void onRemoveNotes(const QList<int> &notesId);
    void onInsertNote(Note *note);
    void onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey);
    void onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const;
    void onResizeNotesRight(const QList<int> &notesId, int deltaTick) const;
    void onAdjustPhonemeOffset(int noteId, PhonemeInfoSeperated::PhonemeType type,const QList<int> &offsets) const;
    void selectNotes(const QList<int> &notesId, bool unselectOther);
    void unselectNotes(const QList<int> &notesId);
    void onOriginalPitchChanged(const QList<Curve *> &curves) const;
    void onPitchEdited(const QList<Curve *> &curves) const;
    void onNotePropertiesEdited(int noteId, const NoteDialogResult &result);
    void onDeleteSelectedNotes();
    void onSelectAllNotes();
    void onFillLyric(QWidget *parent);

private:
    Q_DECLARE_PRIVATE(ClipEditorViewController);
    ClipEditorViewControllerPrivate *d_ptr;
};



#endif // CLIPEDITVIEWCONTROLLER_H
