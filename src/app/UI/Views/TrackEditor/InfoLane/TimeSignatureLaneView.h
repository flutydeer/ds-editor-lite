#ifndef TIMESIGNATURELANEVIEW_H
#define TIMESIGNATURELANEVIEW_H

#include "InfoLaneView.h"

// Info lane showing the project's time signature points as chips ("3/4") at
// their bar lines. Editing goes through a modal dialog and AppController, so
// every confirmed change is one undoable operation:
//   - double click on a chip: edit that point
//   - double click on blank: insert a point at the clicked bar
//   - context menu on a chip: edit or remove it (bar 0 anchor is not removable)
class TimeSignatureLaneView final : public InfoLaneView {
    Q_OBJECT

public:
    explicit TimeSignatureLaneView(QWidget *parent = nullptr);

protected:
    void chipDoubleClicked(const Chip &chip) override;
    void blankDoubleClicked(const QPoint &pos) override;
    void chipContextMenuRequested(const Chip &chip, const QPoint &globalPos) override;

private:
    void rebuildChips();
    void openEditorFor(int barIndex);
};

#endif // TIMESIGNATURELANEVIEW_H
