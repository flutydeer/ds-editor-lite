#include "TempoLaneView.h"

#include "Controller/AppController.h"
#include "UI/Dialogs/Timeline/EditTempoDialog.h"
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QMenu>

TempoLaneView::TempoLaneView(QWidget *parent) : InfoLaneView(parent) {
    setObjectName("tempoLaneView");

    connect(appModel, &AppModel::modelChanged, this, &TempoLaneView::rebuildChips);
    connect(appModel, &AppModel::timelineChanged, this, &TempoLaneView::rebuildChips);
    rebuildChips();
}

void TempoLaneView::chipDoubleClicked(const Chip &chip) {
    openEditorFor(chip.id);
}

void TempoLaneView::blankDoubleClicked(const QPoint &pos) {
    openEditorFor(snappedTickAt(pos.x()));
}

void TempoLaneView::chipContextMenuRequested(const Chip &chip, const QPoint &globalPos) {
    const int tick = chip.id;
    QMenu menu(this);
    menu.addAction(tr("Edit Tempo..."), this, [this, tick] { openEditorFor(tick); });
    const auto removeAction =
        menu.addAction(tr("Remove Tempo"), this, [tick] { appController->onRemoveTempoAt(tick); });
    removeAction->setEnabled(tick != 0);
    menu.exec(globalPos);
}

void TempoLaneView::rebuildChips() {
    QList<Chip> chips;
    for (const auto &tempo : appModel->timeline().tempos()) {
        chips.append({tempo.pos, tempo.pos, QStringLiteral("%1 BPM").arg(tempo.value, 0, 'f', 3)});
    }
    setChips(std::move(chips));
}

void TempoLaneView::openEditorFor(const int tick) {
    EditTempoDialog dialog(tick, appModel->timeline().tempoAt(tick), window());
    if (dialog.exec() != QDialog::Accepted)
        return;
    appController->onSetTempoAt(tick, dialog.tempo());
}

int TempoLaneView::snappedTickAt(const double x) const {
    const int rawTick = qMax(0, qRound(xToTick(x)));
    const double ticksPerPixel = width() > 0 ? (xToTick(width()) - xToTick(0)) / width() : 0.0;
    const int gridStep = logicalGridStepForScale(ticksPerPixel, rawTick);
    return qMax(0, TimelineSnapUtils::snapNearest(rawTick, gridStep));
}
