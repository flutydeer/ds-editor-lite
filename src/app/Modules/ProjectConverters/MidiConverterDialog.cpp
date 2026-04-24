#include "MidiConverterDialog.h"

#include <tuple>

#include <QCheckBox>

#include <QFormLayout>
#include <QGroupBox>
#include <QTextCodec>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPlainTextEdit>

#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"

// Convert note number to note name.
static QString ToneNumToToneName(const int num) {
    static const QString tones[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    };

    int step = num % 12;
    int octave = num / 12 - 1;

    if (num < 0) {
        octave -= 1;
        step = (step + 12) % 12;
    }

    // Verify the tone and octave
    if (octave < -1 || step < 0 || step >= 12) {
        return "Invalid tone or octave";
    }

    return tones[step] + QString::number(octave);
}

class MidiConverterDialogPrivate {
    Q_DECLARE_PUBLIC(MidiConverterDialog)
public:
    MidiConverterDialog *q_ptr{};

    QList<QDspx::MidiIntermediateData::Track> trackInfoList;

    QTextCodec *selectedCodec{};

    bool detectIsUtf8() const {
        QByteArray data;
        for (const auto &trackInfo : trackInfoList) {
            const auto notes = trackInfo.notes;
            for (const auto &note : notes)
                data += note.lyric + "";
            if (data.isEmpty())
                continue;
            QTextCodec::ConverterState state;
            std::ignore =
                QTextCodec::codecForName("UTF-8")->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            data = trackInfo.title;
            std::ignore =
                QTextCodec::codecForName("UTF-8")->toUnicode(data.data(), data.size(), &state);
            if (state.invalidChars)
                return false;
            return true;
        }
        return false;
    }

    bool detectIsSystemEncoding() const {
        QByteArray data;
        for (const auto &trackInfo : trackInfoList) {
            const auto notes = trackInfo.notes;
            for (const auto &note : notes)
                data += note.lyric + "";
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

    QString computeTrackItemText(const QDspx::MidiIntermediateData::Track &trackInfo) const {
        std::set<qint32> staticKeyNum;
        for (const auto &note : trackInfo.notes)
            staticKeyNum.insert(note.key);
        const auto keyRange = staticKeyNum.empty()
                                  ? "-"
                                  : ToneNumToToneName(*staticKeyNum.begin()) + "-" +
                                        ToneNumToToneName(*staticKeyNum.rbegin());
        return MidiConverterDialog::tr("Track %1: %n note(s) (%2)", "", trackInfo.notes.count())
            .arg(selectedCodec->toUnicode(trackInfo.title), keyRange);
    }

    ComboBox *codecComboBox{};
    QTreeWidgetItem *parentItem{};

    void detectCodec() {
        if (detectIsUtf8()) {
            codecComboBox->setCurrentIndex(0);
            codecComboBox->setItemText(codecComboBox->currentIndex(),
                                       codecComboBox->currentText() +
                                           MidiConverterDialog::tr(" (auto detected)"));
            selectedCodec = QTextCodec::codecForName("UTF-8");
        } else if (detectIsSystemEncoding()) {
            codecComboBox->setCurrentIndex(1);
            codecComboBox->setItemText(codecComboBox->currentIndex(),
                                       codecComboBox->currentText() +
                                           MidiConverterDialog::tr(" (auto detected)"));
            selectedCodec = QTextCodec::codecForLocale();
        } else {
            codecComboBox->setCurrentIndex(0);
            selectedCodec = QTextCodec::codecForName("UTF-8");
        }
    }

    void updateTrackSelector() const {
        while (parentItem->childCount())
            parentItem->removeChild(parentItem->child(0));
        for (int i = 0; i < trackInfoList.size(); i++) {
            const auto &trackInfo = trackInfoList.at(i);
            const auto item = new QTreeWidgetItem;
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, i);
            item->setText(0, computeTrackItemText(trackInfo));
            parentItem->addChild(item);
            if (trackInfo.notes.count())
                item->setCheckState(0, Qt::Checked);
        }
    }

    void updateText() const {
        for (int i = 0; i < parentItem->childCount(); i++) {
            const auto item = parentItem->child(i);
            const auto &trackInfo = trackInfoList.at(i);
            item->setText(0, computeTrackItemText(trackInfo));
        }
    }

    bool useMidiTimeline = true;
};

MidiConverterDialog::MidiConverterDialog(
    const QList<QDspx::MidiIntermediateData::Track> &trackInfoList, QWidget *parent)
    : Dialog(parent), d_ptr(new MidiConverterDialogPrivate) {
    Q_D(MidiConverterDialog);
    d->q_ptr = this;

    setWindowTitle(tr("Configure MIDI Import"));
    // setTitle(tr("Configure MIDI Import"));

    auto contentLayout = new QVBoxLayout(body());

    const auto formLayout = new QFormLayout;
    d->codecComboBox = new ComboBox;
    formLayout->addRow("&Encoding", d->codecComboBox);

    const auto trackLayout = new QVBoxLayout;

    const auto trackSelectorGroupBox = new QGroupBox(tr("Track Selector"));
    const auto trackSelectorLayout = new QVBoxLayout;
    const auto trackSelector = new QTreeWidget;
    trackSelector->setHeaderHidden(true);
    trackSelector->setRootIsDecorated(false);
    trackSelectorLayout->addWidget(trackSelector);
    trackSelectorGroupBox->setLayout(trackSelectorLayout);
    trackLayout->addWidget(trackSelectorGroupBox, 2);

    const auto previewGroupBox = new QGroupBox(tr("Lyrics Preview"));
    const auto previewLayout = new QVBoxLayout;
    const auto previewTextEdit = new QPlainTextEdit;
    previewTextEdit->setReadOnly(true);
    previewLayout->addWidget(previewTextEdit);
    previewGroupBox->setLayout(previewLayout);
    trackLayout->addWidget(previewGroupBox, 1);

    formLayout->addRow(trackLayout);

    const auto useMidiTimelineCheckBox =
        new QCheckBox(tr("&Import tempo and time signature from MIDI"));
    formLayout->addRow(useMidiTimelineCheckBox);

    contentLayout->addLayout(formLayout);

    auto okButton = new AccentButton(tr("OK"));
    setPositiveButton(okButton);
    auto cancelButton = new Button(tr("Cancel"));
    setNegativeButton(cancelButton);

    const auto localCodecName = QTextCodec::codecForLocale()->name();

    d->codecComboBox->addItem(QStringLiteral("UTF-8"), "UTF-8");
    d->codecComboBox->addItem(tr("System"), localCodecName);

    for (const auto codec : QTextCodec::availableMibs()) {
        auto codecName = QTextCodec::codecForMib(codec)->name();
        if (codecName == "UTF-8" || codecName == localCodecName)
            continue;
        d->codecComboBox->addItem(codecName, codecName);
    }

    d->parentItem = new QTreeWidgetItem({tr("Select All")});
    d->parentItem->setFlags(d->parentItem->flags() & ~Qt::ItemIsSelectable |
                            Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
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
            for (const auto &note :
                 d->trackInfoList.at(selection[0]->data(0, Qt::UserRole).toInt()).notes) {
                lyrics.append(d->selectedCodec->toUnicode(note.lyric));
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

    connect(d->codecComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
            [=](const int index) {
                d->selectedCodec =
                    QTextCodec::codecForName(d->codecComboBox->itemData(index).toByteArray());
                d->updateText();
                updateLyricsPreview();
            });

    connect(trackSelector, &QTreeWidget::itemSelectionChanged, this, updateLyricsPreview);

    useMidiTimelineCheckBox->setChecked(true);
    connect(useMidiTimelineCheckBox, &QAbstractButton::clicked, this,
            [=](const bool checked) { d->useMidiTimeline = checked; });

    trackSelector->expandAll();
    trackSelector->setItemsExpandable(false);

    connect(okButton, &QAbstractButton::clicked, this, &Dialog::accept);
    connect(cancelButton, &QAbstractButton::clicked, this, &Dialog::reject);

    resize(800, 400);

    setTrackInfoList(trackInfoList);

    if (trackSelector->topLevelItemCount() > 0) {
        const auto topItem = trackSelector->topLevelItem(0);
        if (topItem->childCount() > 0)
            trackSelector->setCurrentItem(topItem->child(0));
    }
}

MidiConverterDialog::~MidiConverterDialog() = default;

void MidiConverterDialog::setTrackInfoList(
    const QList<QDspx::MidiIntermediateData::Track> &trackInfoList) {
    Q_D(MidiConverterDialog);
    d->trackInfoList = trackInfoList;
    d->detectCodec();
    d->updateTrackSelector();
}

QList<QDspx::MidiIntermediateData::Track> MidiConverterDialog::trackInfoList() const {
    Q_D(const MidiConverterDialog);
    return d->trackInfoList;
}

QList<int> MidiConverterDialog::selectedTracks() const {
    Q_D(const MidiConverterDialog);
    QList<int> ret;
    for (int i = 0; i < d->parentItem->childCount(); i++) {
        const auto item = d->parentItem->child(i);
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
