#ifndef TEMPOLANEVIEW_H
#define TEMPOLANEVIEW_H

#include "InfoLaneView.h"

// Displays the tempo map as fixed staircase breakpoints. Points can be
// inserted, edited, and removed, but are intentionally never draggable.
class TempoLaneView final : public InfoLaneView {
    Q_OBJECT

public:
    explicit TempoLaneView(QWidget *parent = nullptr);

protected:
    void chipDoubleClicked(const Chip &chip) override;
    void blankDoubleClicked(const QPoint &pos) override;
    void chipContextMenuRequested(const Chip &chip, const QPoint &globalPos) override;

private:
    void rebuildChips();
    void openEditorFor(int tick);
    [[nodiscard]] int snappedTickAt(double x) const;
};

#endif // TEMPOLANEVIEW_H
