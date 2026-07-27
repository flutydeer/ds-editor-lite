#include "TempoLaneView.h"

#include "Controller/AppController.h"
#include "UI/Dialogs/Timeline/EditTempoDialog.h"
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>

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
    Menu menu(this);

    auto *editAction =
        menu.addAction(tr("Edit Tempo..."), this, [this, tick] { openEditorFor(tick); });
    editAction->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/edit_16_regular.svg")));

    const auto removeAction =
        menu.addAction(tr("Remove Tempo"), this, [tick] { appController->onRemoveTempoAt(tick); });
    removeAction->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    removeAction->setEnabled(tick != 0);

    menu.exec(globalPos);
}

void TempoLaneView::rebuildChips() {
    QList<Chip> chips;
    for (const auto &tempo : appModel->timeline().tempos()) {
        chips.append({tempo.pos, tempo.pos, Tempo::formatValue(tempo.value)});
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
