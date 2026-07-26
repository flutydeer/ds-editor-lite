#include "TimeSignatureLaneView.h"

#include "Controller/AppController.h"
#include "UI/Dialogs/Timeline/EditTimeSignatureDialog.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QMenu>

TimeSignatureLaneView::TimeSignatureLaneView(QWidget *parent) : InfoLaneView(parent) {
    setObjectName("timeSignatureLaneView");

    connect(appModel, &AppModel::modelChanged, this, &TimeSignatureLaneView::rebuildChips);
    connect(appModel, &AppModel::timelineChanged, this, &TimeSignatureLaneView::rebuildChips);
    rebuildChips();
}

void TimeSignatureLaneView::chipDoubleClicked(const Chip &chip) {
    openEditorFor(chip.id);
}

void TimeSignatureLaneView::blankDoubleClicked(const QPoint &pos) {
    // Insert at the bar under the mouse, mirroring how double-clicking blank
    // track area creates a new clip at the clicked position
    const int tick = qMax(0, qRound(xToTick(pos.x())));
    openEditorFor(appModel->timeline().tickToTime(tick).measure);
}

void TimeSignatureLaneView::chipContextMenuRequested(const Chip &chip, const QPoint &globalPos) {
    const int barIndex = chip.id;
    QMenu menu(this);
    menu.addAction(tr("Edit Time Signature..."), this,
                   [this, barIndex] { openEditorFor(barIndex); });
    const auto removeAction = menu.addAction(
        tr("Remove Time Signature"), this,
        [barIndex] { appController->onRemoveTimeSignatureAt(barIndex); });
    // The bar 0 anchor point can be edited but never removed
    removeAction->setEnabled(barIndex != 0);
    menu.exec(globalPos);
}

void TimeSignatureLaneView::rebuildChips() {
    const auto &timeline = appModel->timeline();
    QList<Chip> chips;
    for (const auto &signature : timeline.timeSignatures())
        chips.append({signature.barIndex, timeline.barToTick(signature.barIndex),
                      QStringLiteral("%1/%2")
                          .arg(signature.numerator)
                          .arg(signature.denominator)});
    setChips(std::move(chips));
}

void TimeSignatureLaneView::openEditorFor(const int barIndex) {
    // Prefill with the signature governing this bar; confirming inserts a new
    // point when none exists exactly at the bar yet, or edits the existing one
    const auto signature = appModel->timeline().timeSignatureAt(barIndex);
    EditTimeSignatureDialog dialog(barIndex, signature.numerator, signature.denominator, window());
    if (dialog.exec() != QDialog::Accepted)
        return;
    appController->onSetTimeSignatureAt(barIndex, dialog.numerator(), dialog.denominator());
}
