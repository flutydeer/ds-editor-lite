#ifndef CLIPCONTROLLER_H
#define CLIPCONTROLLER_H

#define clipController ClipController::instance()

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/Params.h>
#include "Automation/ProjectAutomationDtos.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include <lite/Core/Singleton.h>

#include <QObject>

#include <optional>


class Curve;
class ClipControllerPrivate;

class ClipController final : public QObject {
    Q_OBJECT

private:
    explicit ClipController(QObject *parent = nullptr);
    ~ClipController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ClipController)
    Q_DISABLE_COPY_MOVE(ClipController)

public:
    [[nodiscard]] Clip *clip();
    void setClip(Clip *clip);
    bool copySelectedNotesWithParams() const;
    void cutSelectedNotesWithParams();
    void pasteNotesWithParams(const NotesParamsInfo &info, int tick);

    [[nodiscard]] bool canSelectAll() const;
    [[nodiscard]] bool hasSelectedNotes() const;
    [[nodiscard]] bool canShiftWordProperties(const QList<int> &selectedNoteIds) const;
    [[nodiscard]] std::optional<Automation::NoteId> onInsertNote(Automation::NoteDraftDto note);
    [[nodiscard]] std::optional<Automation::NoteId> onSplitNote(Automation::NoteId noteId,
                                                                Automation::NoteDraftDto newNote,
                                                                int newLength) const;

signals:
    // TODO: 连接到 AppStatus 模型监听更改
    void canSelectAllChanged(bool canSelectAll);
    void hasSelectedNotesChanged(bool has);

public slots:
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    void onRemoveNotes(const QList<int> &notesId);
    void onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey);
    void onResizeNotesLeft(const QList<int> &notesId, int deltaTick, int minimumLength) const;
    void onResizeNotesRight(const QList<int> &notesId, int deltaTick, int minimumLength) const;
    void onAdjustPhonemeOffset(int noteId, const QList<int> &offsets) const;
    void onResetPhonemeOffsets(QWidget *parent) const;
    void selectNotes(const QList<int> &notesId, bool unselectOther);
    void unselectNotes(const QList<int> &notesId);
    void onParamEdited(ParamInfo::Name name, const QList<Curve *> &curves) const;
    void onQuantizeNotes(int quantize, bool quantizeStart, bool quantizeLength) const;
    void onNoteLanguagesEdited(const QList<int> &noteIds, const QString &language);
    void onNoteLyricEdited(int noteId, const QString &lyric);
    void onNotePronunciationEdited(int noteId, const QString &pronunciation);
    void onShiftWordPropertiesBackward(const QList<int> &selectedNoteIds);
    void onNotePhonemesEdited(int noteId, const QList<PhonemeName> &phonemeNames);
    void onDeleteSelectedNotes();
    void onSelectAllNotes();
    void onFillLyric(QWidget *parent);
    void onSearchLyric(QWidget *parent);

private:
    Q_DECLARE_PRIVATE(ClipController);
    ClipControllerPrivate *d_ptr;
};



#endif // CLIPCONTROLLER_H
