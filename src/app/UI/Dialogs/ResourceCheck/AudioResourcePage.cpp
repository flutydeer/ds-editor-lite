#include "AudioResourcePage.h"

#include "Controller/AudioDecodingController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Track.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Controls/Button.h"

#include <TalcsWidgets/AudioFileDialog.h>

#include <QDataStream>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIODevice>
#include <QJsonObject>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
    constexpr int kClipIdRole = Qt::UserRole;
    constexpr int kStatusRole = Qt::UserRole + 1;

    QString trackNameForClip(const int clipId) {
        int trackIndex = -1;
        if (appModel->findClipById(clipId, trackIndex) && trackIndex >= 0)
            return appModel->tracks().at(trackIndex)->name();
        return {};
    }
}

AudioResourcePage::AudioResourcePage(const QList<int> &missingClipIds,
                                     const QList<int> &unconfirmedClipIds, QWidget *parent)
    : IResourceCheckPage(parent) {
    m_tree = new QTreeWidget;
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels(
        {tr("File"), tr("Referenced by"), tr("Original location"), tr("Status")});
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    for (const auto clipId : missingClipIds)
        addRow(clipId, RowStatus::Missing);
    for (const auto clipId : unconfirmedClipIds)
        addRow(clipId, RowStatus::Unconfirmed);
    for (int i = 0; i < m_tree->columnCount(); i++)
        m_tree->resizeColumnToContents(i);

    m_btnRelocate = new Button(tr("Relink..."));
    m_btnConfirm = new Button(tr("Confirm"));
    m_btnRelocate->setEnabled(false);
    m_btnConfirm->setEnabled(false);

    const auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_btnRelocate);
    buttonLayout->addWidget(m_btnConfirm);
    buttonLayout->addStretch();

    const auto layout = new QVBoxLayout;
    layout->addWidget(m_tree);
    layout->addLayout(buttonLayout);
    layout->setContentsMargins({});
    setLayout(layout);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            [this] { updateActionButtons(); });
    connect(m_btnRelocate, &Button::clicked, this, &AudioResourcePage::onRelocateClicked);
    connect(m_btnConfirm, &Button::clicked, this, &AudioResourcePage::onConfirmClicked);

    // Refresh the row status in place when cascading resolution recovers a file
    connect(audioDecodingController, &AudioDecodingController::clipRelocated, this,
            [this](const int clipId, const QString &newPath) {
                if (const auto item = findRowByClipId(clipId)) {
                    item->setText(2, newPath);
                    setRowStatus(item, RowStatus::Resolved);
                    updateActionButtons();
                }
            });
}

QString AudioResourcePage::title() const {
    return tr("Audio Files");
}

bool AudioResourcePage::hasPendingIssues() const {
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        const auto status =
            static_cast<RowStatus>(m_tree->topLevelItem(i)->data(0, kStatusRole).toInt());
        if (status != RowStatus::Resolved)
            return true;
    }
    return false;
}

void AudioResourcePage::addRow(const int clipId, const RowStatus status) {
    int trackIndex = -1;
    const auto clip = appModel->findClipById(clipId, trackIndex);
    if (!clip || clip->clipType() != IClip::Audio)
        return;
    const auto audioClip = static_cast<AudioClip *>(clip);

    const auto item = new QTreeWidgetItem;
    item->setText(0, QFileInfo(audioClip->path()).fileName());
    item->setText(1, tr("%1 (track %2)").arg(clip->name(), trackNameForClip(clipId)));
    item->setText(2, audioClip->path());
    item->setData(0, kClipIdRole, clipId);
    setRowStatus(item, status);
    m_tree->addTopLevelItem(item);
}

void AudioResourcePage::setRowStatus(QTreeWidgetItem *item, const RowStatus status) const {
    item->setData(0, kStatusRole, static_cast<int>(status));
    switch (status) {
        case RowStatus::Missing:
            item->setText(3, tr("Missing"));
            break;
        case RowStatus::Unconfirmed:
            item->setText(3, tr("Matched by name, please confirm"));
            break;
        case RowStatus::Resolved:
            item->setText(3, tr("Resolved"));
            break;
    }
}

QTreeWidgetItem *AudioResourcePage::findRowByClipId(const int clipId) const {
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        const auto item = m_tree->topLevelItem(i);
        if (item->data(0, kClipIdRole).toInt() == clipId)
            return item;
    }
    return nullptr;
}

void AudioResourcePage::onRelocateClicked() {
    const auto items = m_tree->selectedItems();
    if (items.isEmpty())
        return;
    const auto item = items.first();
    const int clipId = item->data(0, kClipIdRole).toInt();

    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(AudioContext::instance()->formatManager(),
                                                         fileName, userData, entryClassName, this,
                                                         tr("Select an Audio File"), ".");
    if (fileName.isNull())
        return;

    QByteArray dataBuffer;
    QDataStream o(&dataBuffer, QIODevice::WriteOnly);
    o << userData;
    const QJsonObject workspace{
        {"userData",       QString::fromLatin1(dataBuffer.toBase64())},
        {"entryClassName", entryClassName                            },
    };
    trackController->onRelocateAudioClip(clipId, fileName, io, workspace);

    item->setText(2, fileName);
    setRowStatus(item, RowStatus::Resolved);
    updateActionButtons();

    // Cascade: the other missing files are likely in the same directory
    audioDecodingController->resolveMissingClipsNear(fileName);
}

void AudioResourcePage::onConfirmClicked() {
    const auto items = m_tree->selectedItems();
    if (items.isEmpty())
        return;
    const auto item = items.first();
    TrackController::confirmAudioClipPath(item->data(0, kClipIdRole).toInt());
    setRowStatus(item, RowStatus::Resolved);
    updateActionButtons();
}

void AudioResourcePage::updateActionButtons() const {
    const auto items = m_tree->selectedItems();
    if (items.isEmpty()) {
        m_btnRelocate->setEnabled(false);
        m_btnConfirm->setEnabled(false);
        return;
    }
    const auto status = static_cast<RowStatus>(items.first()->data(0, kStatusRole).toInt());
    m_btnRelocate->setEnabled(status != RowStatus::Resolved);
    m_btnConfirm->setEnabled(status == RowStatus::Unconfirmed);
}
