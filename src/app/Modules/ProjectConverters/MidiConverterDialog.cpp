#include "MidiConverterDialog.h"

#include <tuple>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QTextCodec>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>

class MidiConverterDialogPrivate {
    Q_DECLARE_PUBLIC(MidiConverterDialog)
public:
    MidiConverterDialog *q_ptr;

    QList<QDspx::MidiConverter::TrackInfo> trackInfoList;

    QTextCodec *selectedCodec;

    bool detectIsUtf8() const {
        QByteArray data;
        for (const auto &trackInfo : trackInfoList) {
            data = trackInfo.lyrics.join("");
            if (data.isEmpty())
                continue;
            QTextCodec::ConverterState state;
            std::ignore = QTextCodec::codecForName("UTF-8")->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            data = trackInfo.title;
            std::ignore = QTextCodec::codecForName("UTF-8")->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            return true;
        }
        return false;
    }

    bool detectIsSystemEncoding() const {
        QByteArray data;
        for (const auto &trackInfo : trackInfoList) {
            data = trackInfo.lyrics.join("");
            if (data.isEmpty())
                continue;
            QTextCodec::ConverterState state;
            std::ignore = QTextCodec::codecForLocale()->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            data = trackInfo.title;
            std::ignore = QTextCodec::codecForLocale()->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            return true;
        }
        return false;
    }

    QString computeTrackItemText(const QDspx::MidiConverter::TrackInfo &trackInfo) {
        return MidiConverterDialog::tr("Track %1: %n note(s) (%2)", "", trackInfo.noteCount)
            .arg(selectedCodec->toUnicode(trackInfo.title), trackInfo.keyRange);
    }

    QComboBox *codecComboBox;
    QTreeWidgetItem *parentItem;

    void detectCodec() {
        if (detectIsUtf8()) {
            codecComboBox->setCurrentIndex(0);
            codecComboBox->setItemText(codecComboBox->currentIndex(), codecComboBox->currentText() + MidiConverterDialog::tr(" (auto detected)"));
            selectedCodec = QTextCodec::codecForName("UTF-8");
        } else if (detectIsSystemEncoding()) {
            codecComboBox->setCurrentIndex(1);
            codecComboBox->setItemText(codecComboBox->currentIndex(), codecComboBox->currentText() + MidiConverterDialog::tr(" (auto detected)"));
            selectedCodec = QTextCodec::codecForLocale();
        } else {
            codecComboBox->setCurrentIndex(0);
            selectedCodec = QTextCodec::codecForName("UTF-8");
        }
    }

    void updateTrackSelector() {
        while (parentItem->childCount())
            parentItem->removeChild(parentItem->child(0));
        for (int i = 0; i < trackInfoList.size(); i++) {
            const auto &trackInfo = trackInfoList.at(i);
            auto item = new QTreeWidgetItem;
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, i);
            item->setText(0, computeTrackItemText(trackInfo));
            parentItem->addChild(item);
            if (trackInfo.noteCount)
                item->setCheckState(0, Qt::Checked);
        }
    }

    void updateText() {
        for (int i = 0; i < parentItem->childCount(); i++) {
            auto item = parentItem->child(i);
            const auto &trackInfo = trackInfoList.at(i);
            item->setText(0, computeTrackItemText(trackInfo));
        }
    }

    bool useMidiTimeline = true;
};

MidiConverterDialog::MidiConverterDialog(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList, QWidget *parent) : QDialog(parent), d_ptr(new MidiConverterDialogPrivate) {
    Q_D(MidiConverterDialog);
    d->q_ptr = this;

    setWindowTitle(tr("Configure MIDI Import"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto layout = new QVBoxLayout;

    auto formLayout = new QFormLayout;
    d->codecComboBox = new QComboBox;
    formLayout->addRow("&Encoding", d->codecComboBox);

    auto trackLayout = new QVBoxLayout;

    auto trackSelectorGroupBox = new QGroupBox(tr("Track Selector"));
    auto trackSelectorLayout = new QVBoxLayout;
    auto trackSelector = new QTreeWidget;
    trackSelector->setHeaderHidden(true);
    trackSelector->setRootIsDecorated(false);
    trackSelectorLayout->addWidget(trackSelector);
    trackSelectorGroupBox->setLayout(trackSelectorLayout);
    trackLayout->addWidget(trackSelectorGroupBox, 2);

    auto previewGroupBox = new QGroupBox(tr("Lyrics Preview"));
    auto previewLayout = new QVBoxLayout;
    auto previewTextEdit = new QPlainTextEdit;
    previewTextEdit->setReadOnly(true);
    previewLayout->addWidget(previewTextEdit);
    previewGroupBox->setLayout(previewLayout);
    trackLayout->addWidget(previewGroupBox, 1);

    formLayout->addRow(trackLayout);

    auto useMidiTimelineCheckBox = new QCheckBox(tr("&Import tempo and time signature from MIDI"));
    formLayout->addRow(useMidiTimelineCheckBox);

    layout->addLayout(formLayout);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto okButton = new QPushButton(tr("OK"));
    okButton->setDefault(true);
    buttonLayout->addWidget(okButton);
    auto cancelButton = new QPushButton(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    setLayout(layout);

    auto localCodecName = QTextCodec::codecForLocale()->name();

    d->codecComboBox->addItem(QStringLiteral("UTF-8"), "UTF-8");
    d->codecComboBox->addItem(tr("System"), localCodecName);

    for (auto codec : QTextCodec::availableMibs()) {
        auto codecName = QTextCodec::codecForMib(codec)->name();
        if (codecName == "UTF-8" || codecName == localCodecName)
            continue;
        d->codecComboBox->addItem(codecName, codecName);
    }

    d->parentItem = new QTreeWidgetItem({tr("Select All")});
    d->parentItem->setFlags(d->parentItem->flags() & ~Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    d->parentItem->setCheckState(0, Qt::Unchecked);
    trackSelector->addTopLevelItem(d->parentItem);

    auto updateLyricsPreview = [=] {
        previewTextEdit->clear();
        auto selection = trackSelector->selectedItems();
        if (selection.isEmpty()) {
            previewTextEdit->setPlaceholderText(tr("Select a track to preview its lyrics"));
            previewTextEdit->setAccessibleDescription(tr("Select a track to preview its lyrics"));
        } else {
            QStringList lyrics;
            for (auto lyric : d->trackInfoList.at(selection[0]->data(0, Qt::UserRole).toInt()).lyrics) {
                lyrics.append(d->selectedCodec->toUnicode(lyric));
            }
            if (lyrics.isEmpty()) {
                previewTextEdit->setPlaceholderText(tr("No lyrics in current track"));
                previewTextEdit->setAccessibleDescription(tr("No lyrics in current track"));
            } else {
                previewTextEdit->setPlaceholderText({});
                previewTextEdit->setPlainText(lyrics.join(" "));
            }

        }
    };

    connect(d->codecComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        d->selectedCodec = QTextCodec::codecForName(d->codecComboBox->itemData(index).toByteArray());
        d->updateText();
        updateLyricsPreview();
    });

    connect(trackSelector, &QTreeWidget::itemSelectionChanged, this, updateLyricsPreview);

    useMidiTimelineCheckBox->setChecked(true);
    connect(useMidiTimelineCheckBox, &QAbstractButton::clicked, this, [=](bool checked) {
        d->useMidiTimeline = checked;
    });

    trackSelector->expandAll();
    trackSelector->setItemsExpandable(false);

    connect(okButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);

    resize(800, 400);

    setTrackInfoList(trackInfoList);

}

MidiConverterDialog::~MidiConverterDialog() = default;

void MidiConverterDialog::setTrackInfoList(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList) {
    Q_D(MidiConverterDialog);
    d->trackInfoList = trackInfoList;
    d->detectCodec();
    d->updateTrackSelector();
}

QList<QDspx::MidiConverter::TrackInfo> MidiConverterDialog::trackInfoList() const {
    Q_D(const MidiConverterDialog);
    return d->trackInfoList;
}

QList<int> MidiConverterDialog::selectedTracks() const {
    Q_D(const MidiConverterDialog);
    QList<int> ret;
    for (int i = 0; i < d->parentItem->childCount(); i++) {
        auto item = d->parentItem->child(i);
        if (item->checkState(0) == Qt::Checked) {
            ret.append(i);
        }
    }
    return ret;
}

QTextCodec *MidiConverterDialog::selectedCodec() const {
    Q_D(const MidiConverterDialog);
    return d->selectedCodec;
}

bool MidiConverterDialog::useMidiTimeline() const {
    Q_D(const MidiConverterDialog);
    return d->useMidiTimeline;
}
