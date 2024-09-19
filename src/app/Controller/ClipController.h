//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPCONTROLLER_H
#define CLIPCONTROLLER_H

#define clipController ClipController::instance()

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Params.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Model/NoteDialogResult/NoteDialogResult.h"
#include "Utils/Singleton.h"

#include <QObject>


class Curve;
class ClipControllerPrivate;
class IClipEditorView;

class ClipController final : public QObject, public Singleton<ClipController> {
    Q_OBJECT

public:
    explicit ClipController();
    ~ClipController() override;
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
    void onAdjustPhonemeOffset(int noteId, Phonemes::Type type, const QList<int> &offsets) const;
    void selectNotes(const QList<int> &notesId, bool unselectOther);
    void unselectNotes(const QList<int> &notesId);
    void onParamEdited(ParamInfo::Name name, const QList<Curve *> &curves) const;
    void onNotePropertiesEdited(int noteId, const NoteDialogResult &result);
    void onDeleteSelectedNotes();
    void onSelectAllNotes();
    void onFillLyric(QWidget *parent);

private:
    Q_DECLARE_PRIVATE(ClipController);
    ClipControllerPrivate *d_ptr;
};



#endif // CLIPCONTROLLER_H
