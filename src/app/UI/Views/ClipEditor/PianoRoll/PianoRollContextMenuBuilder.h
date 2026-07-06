#ifndef PIANOROLLCONTEXTMENUBUILDER_H
#define PIANOROLLCONTEXTMENUBUILDER_H

#include <functional>

class Menu;
class NoteView;
class QPoint;
class PianoRollGraphicsView;
class PianoRollSelectionModel;

class PianoRollContextMenuBuilder {
public:
    PianoRollContextMenuBuilder() = delete;

    static Menu *buildNoteContextMenu(PianoRollGraphicsView *view,
                                      NoteView *noteView,
                                      std::function<void()> onDeleteNotes,
                                      std::function<void(int noteId)> onOpenProperties);
    static Menu *buildBackgroundContextMenu(PianoRollGraphicsView *view,
                                            PianoRollSelectionModel *selectionModel,
                                            const QPoint &pos,
                                            int offset);
};

#endif // PIANOROLLCONTEXTMENUBUILDER_H