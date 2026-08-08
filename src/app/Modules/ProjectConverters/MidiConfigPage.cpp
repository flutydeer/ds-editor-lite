#include "MidiConfigPage.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QItemSelectionModel>
#include <QLocale>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/ProjectConverters/MidiTextCodecConverter.h>

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

class MidiConfigPagePrivate {
    Q_DECLARE_PUBLIC(MidiConfigPage)
public:
    explicit MidiConfigPagePrivate(MidiConfigPage *q) : q_ptr(q) {
        currentCodec = MidiTextCodecConverter::defaultCodec();
    }

    void init() {
        Q_Q(MidiConfigPage);

        auto *contentLayout = new QVBoxLayout(q);

        auto *codecLayout = new QHBoxLayout;
        auto *codecLabel = new QLabel(MidiConfigPage::tr("Encoding:"), q);
        codecComboBox = new ComboBox(q);
        codecLayout->addWidget(codecLabel);
        codecLayout->addWidget(codecComboBox, 1);
        contentLayout->addLayout(codecLayout);

        populateCodecCombo();
        syncComboToCodec();

        QObject::connect(codecComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), q,
                         [this](int index) {
                             if (index < 0)
                                 return;
                             const QByteArray codecId =
                                 codecComboBox->itemData(index).toByteArray();
                             Q_Q(MidiConfigPage);
                             q->setSelectedCodec(codecId);
                         });

        auto *selectorGroup = new QGroupBox(MidiConfigPage::tr("Track Selector"), q);
        auto *selectorLayout = new QVBoxLayout(selectorGroup);

        auto *buttonLayout = new QHBoxLayout;
        selectAllCheckBox = new BinaryToggleCheckBox(MidiConfigPage::tr("Select All"), q);
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

        contentLayout->addWidget(selectorGroup, 2);

        auto *previewGroup = new QGroupBox(MidiConfigPage::tr("Lyrics Preview"), q);
        auto *previewLayout = new QVBoxLayout(previewGroup);
        previewEdit = new QTextEdit(previewGroup);
        previewEdit->setReadOnly(true);
        previewEdit->setLineWrapMode(QTextEdit::WidgetWidth);
        previewLayout->addWidget(previewEdit);
        contentLayout->addWidget(previewGroup, 1);

        auto *optionsGroup = new QGroupBox(MidiConfigPage::tr("Options"), q);
        auto *optionsLayout = new QVBoxLayout(optionsGroup);

        separateMidiChannelsCheckBox =
            new QCheckBox(MidiConfigPage::tr("Separate MIDI channels"), optionsGroup);
        separateMidiChannelsCheckBox->setChecked(separateMidiChannels);
        optionsLayout->addWidget(separateMidiChannelsCheckBox);

        importTempoCheckBox = new QCheckBox(MidiConfigPage::tr("Import tempo"), optionsGroup);
        importTempoCheckBox->setChecked(importTempo);
        optionsLayout->addWidget(importTempoCheckBox);

        importTimeSignatureCheckBox =
            new QCheckBox(MidiConfigPage::tr("Import time signature"), optionsGroup);
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

        QObject::connect(separateMidiChannelsCheckBox, &QCheckBox::toggled, q,
                         [this](bool checked) {
                             Q_Q(MidiConfigPage);
                             q->setSeparateMidiChannels(checked);
                         });

        QObject::connect(importTempoCheckBox, &QCheckBox::toggled, q, [this](bool checked) {
            Q_Q(MidiConfigPage);
            q->setImportTempo(checked);
        });

        QObject::connect(importTimeSignatureCheckBox, &QCheckBox::toggled, q, [this](bool checked) {
            Q_Q(MidiConfigPage);
            q->setImportTimeSignature(checked);
        });
    }

    void populateCodecCombo() {
        if (!codecComboBox)
            return;
        codecComboBox->clear();
        const auto codecs = MidiTextCodecConverter::availableCodecs();
        for (const auto &codec : codecs) {
            QString itemText = codec.displayName;
            if (!autoDetectedCodec.isEmpty() && codec.identifier == autoDetectedCodec) {
                itemText = MidiConfigPage::tr("%1 (auto detected)").arg(codec.displayName);
            }
            codecComboBox->addItem(itemText, codec.identifier);
        }
    }

    void rebuildModel() {
        Q_Q(MidiConfigPage);

        auto *newModel = new QStandardItemModel(q);
        newModel->setColumnCount(3);
        newModel->setHorizontalHeaderLabels({MidiConfigPage::tr("Name"),
                                             MidiConfigPage::tr("Range"),
                                             MidiConfigPage::tr("Note Count")});

        const int count = trackInfos.size();
        for (int i = 0; i < count; ++i) {
            const auto &info = trackInfos.at(i);

            auto *nameItem = new QStandardItem(decodedName(info.name));
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

            auto *rangeItem = new QStandardItem(info.rangeText);
            rangeItem->setFlags((rangeItem->flags() | Qt::ItemIsSelectable) &
                                ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));

            auto *noteCountItem = new QStandardItem(QLocale().toString(info.noteCount));
            noteCountItem->setFlags((noteCountItem->flags() | Qt::ItemIsSelectable) &
                                    ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));

            newModel->appendRow({nameItem, rangeItem, noteCountItem});
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

        QObject::connect(trackView->selectionModel(), &QItemSelectionModel::currentRowChanged, q,
                         [this](const QModelIndex &current, const QModelIndex &) {
                             updatePreviewForRow(current.row());
                         });

        if (model->rowCount() > 0)
            trackView->setCurrentIndex(model->index(0, 0));

        updateSelectedIndexes();
        updateSelectAllCheckBox();
        updatePreviewForRow(trackView->currentIndex().row());
    }

    void updateNamesForCodec() {
        if (!model)
            return;
        const int rowCount = model->rowCount();
        for (int row = 0; row < rowCount; ++row) {
            auto *item = model->item(row, 0);
            if (!item)
                continue;
            const auto &info = trackInfos.value(row);
            item->setText(decodedName(info.name));
        }
    }

    void updatePreviewForRow(int row) {
        if (!previewEdit)
            return;
        if (row < 0 || row >= trackInfos.size()) {
            previewEdit->clear();
            previewEdit->setPlaceholderText(
                MidiConfigPage::tr("Select a track to preview its lyrics"));
            return;
        }
        const auto &info = trackInfos.at(row);
        const auto lyricText = decodeLyrics(info.lyrics);
        if (lyricText.isEmpty()) {
            previewEdit->clear();
            previewEdit->setPlaceholderText(MidiConfigPage::tr("No lyrics in current track"));
        } else {
            previewEdit->setPlainText(lyricText);
        }
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
        Q_Q(MidiConfigPage);
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

    QString decodeBytes(const QByteArray &bytes) const {
        const auto text = MidiTextCodecConverter::decode(bytes, currentCodec);
        return text.isEmpty() ? QString::fromUtf8(bytes) : text;
    }

    QString decodedName(const QByteArray &bytes) const {
        return decodeBytes(bytes);
    }

    QString decodeLyrics(const QList<QByteArray> &lyrics) const {
        QStringList words;
        words.reserve(lyrics.size());
        for (const auto &word : lyrics)
            words.append(decodeBytes(word));
        return words.join(QLatin1Char(' '));
    }

    QByteArray aggregateLyricsForDetection() const {
        QByteArray combined;
        for (const auto &info : trackInfos) {
            QByteArray lyricsJoined;
            for (int i = 0; i < info.lyrics.size(); ++i) {
                if (i > 0)
                    lyricsJoined.append(' ');
                lyricsJoined.append(info.lyrics.at(i));
            }
            QByteArray trackData = info.name;
            trackData.append(lyricsJoined);
            combined.append(trackData);
        }
        return combined;
    }

    void syncComboToCodec() {
        if (!codecComboBox)
            return;
        const int count = codecComboBox->count();
        for (int i = 0; i < count; ++i) {
            if (codecComboBox->itemData(i).toByteArray() == currentCodec) {
                codecComboBox->setCurrentIndex(i);
                return;
            }
        }
    }

    MidiConfigPage *q_ptr{};
    QList<MidiImportTrackInfo> trackInfos;
    QByteArray currentCodec;
    QByteArray autoDetectedCodec;
    QList<int> selectedIndexesCache;
    bool separateMidiChannels = true;
    bool importTempo = true;
    bool importTimeSignature = true;

    ComboBox *codecComboBox = nullptr;
    QTreeView *trackView = nullptr;
    QStandardItemModel *model = nullptr;
    QTextEdit *previewEdit = nullptr;
    BinaryToggleCheckBox *selectAllCheckBox = nullptr;
    QCheckBox *separateMidiChannelsCheckBox = nullptr;
    QCheckBox *importTempoCheckBox = nullptr;
    QCheckBox *importTimeSignatureCheckBox = nullptr;
};

MidiConfigPage::MidiConfigPage(const QList<MidiImportTrackInfo> &trackInfoList, QWidget *parent)
    : QWidget(parent), d_ptr(new MidiConfigPagePrivate(this)) {
    Q_D(MidiConfigPage);
    d->init();
    setTrackInfoList(trackInfoList);
}

MidiConfigPage::~MidiConfigPage() = default;

QWidget *MidiConfigPage::widget() {
    return this;
}

void MidiConfigPage::setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList) {
    Q_D(MidiConfigPage);
    d->trackInfos = trackInfoList;
    d->autoDetectedCodec.clear();
    d->populateCodecCombo();
    d->rebuildModel();
}

QList<MidiImportTrackInfo> MidiConfigPage::trackInfoList() const {
    Q_D(const MidiConfigPage);
    return d->trackInfos;
}

QList<int> MidiConfigPage::selectedTracks() const {
    Q_D(const MidiConfigPage);
    return d->selectedIndexesCache;
}

QByteArray MidiConfigPage::selectedCodec() const {
    Q_D(const MidiConfigPage);
    return d->currentCodec;
}

bool MidiConfigPage::separateMidiChannels() const {
    Q_D(const MidiConfigPage);
    return d->separateMidiChannels;
}

bool MidiConfigPage::importTempo() const {
    Q_D(const MidiConfigPage);
    return d->importTempo;
}

bool MidiConfigPage::importTimeSignature() const {
    Q_D(const MidiConfigPage);
    return d->importTimeSignature;
}

void MidiConfigPage::detectCodec() {
    Q_D(MidiConfigPage);
    d->autoDetectedCodec = MidiTextCodecConverter::detectEncoding(d->aggregateLyricsForDetection());
    d->populateCodecCombo();
    if (!d->autoDetectedCodec.isEmpty()) {
        setSelectedCodec(d->autoDetectedCodec);
    } else {
        setSelectedCodec(MidiTextCodecConverter::defaultCodec());
    }
}

void MidiConfigPage::setSelectedCodec(const QByteArray &codec) {
    Q_D(MidiConfigPage);
    if (codec.isEmpty())
        return;
    if (codec == d->currentCodec)
        return;
    d->currentCodec = codec;
    d->syncComboToCodec();
    d->updateNamesForCodec();
    d->updatePreviewForRow(d->trackView ? d->trackView->currentIndex().row() : -1);
    emit codecChanged(codec);
}

void MidiConfigPage::setSeparateMidiChannels(const bool enabled) {
    Q_D(MidiConfigPage);
    if (d->separateMidiChannels == enabled)
        return;
    d->separateMidiChannels = enabled;
    if (d->separateMidiChannelsCheckBox) {
        QSignalBlocker blocker(d->separateMidiChannelsCheckBox);
        d->separateMidiChannelsCheckBox->setChecked(enabled);
    }
    emit separateMidiChannelsChanged(enabled);
}

void MidiConfigPage::setImportTempo(const bool enabled) {
    Q_D(MidiConfigPage);
    if (d->importTempo == enabled)
        return;
    d->importTempo = enabled;
    if (d->importTempoCheckBox) {
        QSignalBlocker blocker(d->importTempoCheckBox);
        d->importTempoCheckBox->setChecked(enabled);
    }
    emit importTempoChanged(enabled);
}

void MidiConfigPage::setImportTimeSignature(const bool enabled) {
    Q_D(MidiConfigPage);
    if (d->importTimeSignature == enabled)
        return;
    d->importTimeSignature = enabled;
    if (d->importTimeSignatureCheckBox) {
        QSignalBlocker blocker(d->importTimeSignatureCheckBox);
        d->importTimeSignatureCheckBox->setChecked(enabled);
    }
    emit importTimeSignatureChanged(enabled);
}

MidiUserInput MidiConfigPage::collectInput() const {
    Q_D(const MidiConfigPage);
    MidiUserInput input;
    input.encoding.codec = d->currentCodec;
    input.tracks.selectedTrackIndices = d->selectedIndexesCache;
    input.channels.separateChannels = d->separateMidiChannels;
    input.timeline.importTempo = d->importTempo;
    input.timeline.importTimeSignature = d->importTimeSignature;
    return input;
}
