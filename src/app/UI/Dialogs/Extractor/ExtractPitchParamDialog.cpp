//
// Created by fluty on 24-11-13.
//

#include "ExtractPitchParamDialog.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Track.h"
#include "Modules/Language/LangSetting/Controls/G2pListWidget.h"
#include "UI/Controls/AccentButton.h"

#include <QVBoxLayout>

ExtractPitchParamDialog::ExtractPitchParamDialog(const QList<AudioClip *> &clips) {
    setWindowTitle(tr("Extract Pitch Parameter"));
    setTitle(tr("Select an audio clip"));
    setMinimumWidth(480);

    clipList = new QListWidget;
    clipList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (auto clip : clips) {
        auto item = new QListWidgetItem(QString("%1 (%2)").arg(clip->name(), clip->path()));
        item->setData(Qt::UserRole, clip->id());
        clipList->addItem(item);
    }
    auto layout = new QVBoxLayout;
    layout->addWidget(clipList);
    layout->setContentsMargins({});
    body()->setLayout(layout);
    okButton()->setEnabled(false);

    connect(okButton(), &Button::clicked, this, &Dialog::accept);
    connect(cancelButton(), &Button::clicked, this, [=] {
        selectedClipId = -1;
        reject();
    });
    connect(clipList, &QListWidget::currentRowChanged, this,
            &ExtractPitchParamDialog::onSelectionChanged);
}

void ExtractPitchParamDialog::onSelectionChanged(int row) {
    auto item = clipList->item(row);
    if (!item) {
        selectedClipId = -1;
        okButton()->setEnabled(false);
        return;
    }
    okButton()->setEnabled(true);
    const int clipId = item->data(Qt::UserRole).toInt();
    selectedClipId = clipId;
}