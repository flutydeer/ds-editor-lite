//
// Created by fluty on 24-11-13.
//

#include "ExtractMidiDialog.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Track.h"
#include "Modules/Language/LangSetting/Controls/G2pListWidget.h"
#include "UI/Controls/AccentButton.h"

#include <QVBoxLayout>

ExtractMidiDialog::ExtractMidiDialog(const QList<AudioClip *> &clips) {
    setWindowTitle(tr("Extract Midi Parameter"));
    setTitle(tr("Select an audio clip"));
    setMinimumWidth(480);

    clipList = new QListWidget;
    clipList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (const auto clip : clips) {
        const auto item = new QListWidgetItem(QString("%1 (%2)").arg(clip->name(), clip->path()));
        item->setData(Qt::UserRole, clip->id());
        clipList->addItem(item);
    }
    const auto layout = new QVBoxLayout;
    layout->addWidget(clipList);
    layout->setContentsMargins({});
    body()->setLayout(layout);

    connect(okButton(), &Button::clicked, this, &Dialog::accept);
    connect(cancelButton(), &Button::clicked, this, [=] {
        selectedClipId = -1;
        reject();
    });
    connect(clipList, &QListWidget::currentRowChanged, this,
            &ExtractMidiDialog::onSelectionChanged);
}

void ExtractMidiDialog::onSelectionChanged(const int row) {
    const auto item = clipList->item(row);
    if (!item) {
        selectedClipId = -1;
        return;
    }
    const int clipId = item->data(Qt::UserRole).toInt();
    selectedClipId = clipId;
}