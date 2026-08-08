#include "DspxConfigPage.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLocale>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

class BinaryToggleCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;

protected:
    void nextCheckState() override {
        if (checkState() == Qt::Checked) {
            setCheckState(Qt::Unchecked);
        } else {
            setCheckState(Qt::Checked);
        }
    }
};

class DspxConfigPagePrivate {
    Q_DECLARE_PUBLIC(DspxConfigPage)
public:
    explicit DspxConfigPagePrivate(DspxConfigPage *q) : q_ptr(q) {
    }

    void init() {
        Q_Q(DspxConfigPage);

        auto *contentLayout = new QVBoxLayout(q);

        auto *selectorGroup = new QGroupBox(DspxConfigPage::tr("Track Selector"), q);
        auto *selectorLayout = new QVBoxLayout(selectorGroup);

        auto *buttonLayout = new QHBoxLayout;
        selectAllCheckBox = new BinaryToggleCheckBox(DspxConfigPage::tr("Select All"), q);
        selectAllCheckBox->setTristate(true);
        buttonLayout->addWidget(selectAllCheckBox);
        buttonLayout->addStretch();
        selectorLayout->addLayout(buttonLayout);

        trackView = new QTreeView(selectorGroup);
        trackView->setRootIsDecorated(false);
        trackView->setAlternatingRowColors(true);
        trackView->setSelectionBehavior(QAbstractItemView::SelectRows);
        trackView->setSelectionMode(QAbstractItemView::SingleSelection);
        selectorLayout->addWidget(trackView, 1);

        contentLayout->addWidget(selectorGroup, 1);

        auto *optionsGroup = new QGroupBox(DspxConfigPage::tr("Options"), q);
        auto *optionsLayout = new QVBoxLayout(optionsGroup);

        importTempoCheckBox = new QCheckBox(DspxConfigPage::tr("Import tempo"), optionsGroup);
        importTempoCheckBox->setChecked(importTempo);
        optionsLayout->addWidget(importTempoCheckBox);

        importTimeSignatureCheckBox =
            new QCheckBox(DspxConfigPage::tr("Import time signature"), optionsGroup);
        importTimeSignatureCheckBox->setChecked(importTimeSignature);
        optionsLayout->addWidget(importTimeSignatureCheckBox);

        contentLayout->addWidget(optionsGroup);

        QObject::connect(selectAllCheckBox, &QCheckBox::checkStateChanged, q,
                         [this](Qt::CheckState state) {
                             if (!model)
                                 return;
                             if (state == Qt::PartiallyChecked)
                                 return;
                             QSignalBlocker blocker(model);
                             const int rowCount = model->rowCount();
                             for (int row = 0; row < rowCount; ++row) {
                                 auto *item = model->item(row, 0);
                                 if (!item)
                                     continue;
                                 if (!(item->flags() & Qt::ItemIsEnabled))
                                     continue;
                                 item->setCheckState(state);
                             }
                             if (trackView && trackView->viewport())
                                 trackView->viewport()->update();
                             updateSelectedIndexes();
                         });

        QObject::connect(importTempoCheckBox, &QCheckBox::toggled, q, [this](bool checked) {
            Q_Q(DspxConfigPage);
            q->setImportTempo(checked);
        });

        QObject::connect(importTimeSignatureCheckBox, &QCheckBox::toggled, q, [this](bool checked) {
            Q_Q(DspxConfigPage);
            q->setImportTimeSignature(checked);
        });
    }

    void rebuildModel() {
        Q_Q(DspxConfigPage);

        auto *newModel = new QStandardItemModel(q);
        newModel->setColumnCount(3);
        newModel->setHorizontalHeaderLabels({DspxConfigPage::tr("Name"), DspxConfigPage::tr("Type"),
                                             DspxConfigPage::tr("Note Count")});

        const int count = trackInfos.size();
        for (int i = 0; i < count; ++i) {
            const auto &info = trackInfos.at(i);

            auto *nameItem = new QStandardItem(QString::fromUtf8(info.name));
            nameItem->setCheckable(true);
            Qt::ItemFlags nameFlags =
                Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
            if (info.disabled)
                nameFlags &= ~Qt::ItemIsEnabled;
            nameItem->setFlags(nameFlags);
            if (!info.disabled && info.selectedByDefault) {
                nameItem->setCheckState(Qt::Checked);
            } else {
                nameItem->setCheckState(Qt::Unchecked);
            }

            auto *typeItem = new QStandardItem(info.rangeText);
            typeItem->setFlags((typeItem->flags() | Qt::ItemIsSelectable) &
                               ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));

            auto *noteCountItem = new QStandardItem(QLocale().toString(info.noteCount));
            noteCountItem->setFlags((noteCountItem->flags() | Qt::ItemIsSelectable) &
                                    ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));

            newModel->appendRow({nameItem, typeItem, noteCountItem});
        }

        if (model)
            model->deleteLater();
        model = newModel;
        trackView->setModel(model);
        if (auto *header = trackView->header()) {
            header->setSectionResizeMode(0, QHeaderView::Stretch);
            header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        }

        QObject::connect(model, &QStandardItemModel::itemChanged, q, [this](QStandardItem *item) {
            if (!item)
                return;
            if (item->column() != 0)
                return;
            updateSelectedIndexes();
            updateSelectAllCheckBox();
        });

        updateSelectedIndexes();
        updateSelectAllCheckBox();
    }

    void updateSelectedIndexes() {
        if (!model)
            return;
        QList<int> indexes;
        const int rowCount = model->rowCount();
        indexes.reserve(rowCount);
        for (int row = 0; row < rowCount; ++row) {
            const auto *item = model->item(row, 0);
            if (!item)
                continue;
            if (item->checkState() == Qt::Checked)
                indexes.append(row);
        }
        if (indexes == selectedIndexesCache)
            return;
        selectedIndexesCache = indexes;
        Q_Q(DspxConfigPage);
        emit q->selectedTracksChanged();
    }

    void updateSelectAllCheckBox() {
        if (!selectAllCheckBox || !model)
            return;
        int checkedCount = 0;
        int enabledCount = 0;
        const int rowCount = model->rowCount();
        for (int row = 0; row < rowCount; ++row) {
            const auto *item = model->item(row, 0);
            if (!item)
                continue;
            if (!(item->flags() & Qt::ItemIsEnabled))
                continue;
            enabledCount++;
            if (item->checkState() == Qt::Checked)
                checkedCount++;
        }
        QSignalBlocker blocker(selectAllCheckBox);
        if (enabledCount == 0 || checkedCount == 0) {
            selectAllCheckBox->setCheckState(Qt::Unchecked);
        } else if (checkedCount == enabledCount) {
            selectAllCheckBox->setCheckState(Qt::Checked);
        } else {
            selectAllCheckBox->setCheckState(Qt::PartiallyChecked);
        }
    }

    DspxConfigPage *q_ptr{};
    QList<MidiImportTrackInfo> trackInfos;
    QList<int> selectedIndexesCache;
    bool importTempo = false;
    bool importTimeSignature = false;

    QTreeView *trackView = nullptr;
    QStandardItemModel *model = nullptr;
    BinaryToggleCheckBox *selectAllCheckBox = nullptr;
    QCheckBox *importTempoCheckBox = nullptr;
    QCheckBox *importTimeSignatureCheckBox = nullptr;
};

DspxConfigPage::DspxConfigPage(const QList<MidiImportTrackInfo> &trackInfoList, QWidget *parent)
    : QWidget(parent), d_ptr(new DspxConfigPagePrivate(this)) {
    Q_D(DspxConfigPage);
    d->init();
    setTrackInfoList(trackInfoList);
}

DspxConfigPage::~DspxConfigPage() = default;

QWidget *DspxConfigPage::widget() {
    return this;
}

void DspxConfigPage::setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList) {
    Q_D(DspxConfigPage);
    d->trackInfos = trackInfoList;
    d->rebuildModel();
}

QList<int> DspxConfigPage::selectedTracks() const {
    Q_D(const DspxConfigPage);
    return d->selectedIndexesCache;
}

bool DspxConfigPage::importTempo() const {
    Q_D(const DspxConfigPage);
    return d->importTempo;
}

bool DspxConfigPage::importTimeSignature() const {
    Q_D(const DspxConfigPage);
    return d->importTimeSignature;
}

void DspxConfigPage::setImportTempo(const bool enabled) {
    Q_D(DspxConfigPage);
    if (d->importTempo == enabled)
        return;
    d->importTempo = enabled;
    if (d->importTempoCheckBox) {
        QSignalBlocker blocker(d->importTempoCheckBox);
        d->importTempoCheckBox->setChecked(enabled);
    }
    emit importTempoChanged(enabled);
}

void DspxConfigPage::setImportTimeSignature(const bool enabled) {
    Q_D(DspxConfigPage);
    if (d->importTimeSignature == enabled)
        return;
    d->importTimeSignature = enabled;
    if (d->importTimeSignatureCheckBox) {
        QSignalBlocker blocker(d->importTimeSignatureCheckBox);
        d->importTimeSignatureCheckBox->setChecked(enabled);
    }
    emit importTimeSignatureChanged(enabled);
}

DspxUserInput DspxConfigPage::collectInput() const {
    Q_D(const DspxConfigPage);
    DspxUserInput input;
    input.tracks.selectedTrackIndices = d->selectedIndexesCache;
    input.timeline.importTempo = d->importTempo;
    input.timeline.importTimeSignature = d->importTimeSignature;
    return input;
}
