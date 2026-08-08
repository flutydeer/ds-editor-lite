#include "MidiConverterDialog.h"

#include "MidiConfigPage.h"

#include <QVBoxLayout>

MidiConverterDialog::MidiConverterDialog(const QList<MidiImportTrackInfo> &trackInfoList,
                                         QWidget *parent)
    : Dialog(parent), m_page(new MidiConfigPage(trackInfoList, this)) {
    setWindowTitle(tr("Configure MIDI Import"));
    auto *layout = new QVBoxLayout(body());
    layout->addWidget(m_page);

    connect(m_page, &MidiConfigPage::codecChanged, this,
            [this](const QByteArray &codec) { emit codecChanged(codec); });
    connect(m_page, &MidiConfigPage::selectedTracksChanged, this,
            [this] { emit selectedTracksChanged(); });
    connect(m_page, &MidiConfigPage::separateMidiChannelsChanged, this,
            [this](const bool enabled) { emit separateMidiChannelsChanged(enabled); });
    connect(m_page, &MidiConfigPage::importTempoChanged, this,
            [this](const bool enabled) { emit importTempoChanged(enabled); });
    connect(m_page, &MidiConfigPage::importTimeSignatureChanged, this,
            [this](const bool enabled) { emit importTimeSignatureChanged(enabled); });

    resize(720, 480);
}

MidiConverterDialog::~MidiConverterDialog() = default;

void MidiConverterDialog::setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList) {
    m_page->setTrackInfoList(trackInfoList);
}

QList<MidiImportTrackInfo> MidiConverterDialog::trackInfoList() const {
    return m_page->trackInfoList();
}

QList<int> MidiConverterDialog::selectedTracks() const {
    return m_page->selectedTracks();
}

QByteArray MidiConverterDialog::selectedCodec() const {
    return m_page->selectedCodec();
}

bool MidiConverterDialog::separateMidiChannels() const {
    return m_page->separateMidiChannels();
}

bool MidiConverterDialog::importTempo() const {
    return m_page->importTempo();
}

bool MidiConverterDialog::importTimeSignature() const {
    return m_page->importTimeSignature();
}

void MidiConverterDialog::detectCodec() {
    m_page->detectCodec();
}

void MidiConverterDialog::setSelectedCodec(const QByteArray &codec) {
    m_page->setSelectedCodec(codec);
}

void MidiConverterDialog::setSeparateMidiChannels(const bool enabled) {
    m_page->setSeparateMidiChannels(enabled);
}

void MidiConverterDialog::setImportTempo(const bool enabled) {
    m_page->setImportTempo(enabled);
}

void MidiConverterDialog::setImportTimeSignature(const bool enabled) {
    m_page->setImportTimeSignature(enabled);
}
