#ifndef DS_EDITOR_LITE_MIDICONFIGPAGE_H
#define DS_EDITOR_LITE_MIDICONFIGPAGE_H

#include "Modules/ProjectFormats/IProjectConfigPage.h"
#include "Modules/ProjectFormats/UserInput.h"
#include <lite/ProjectConverters/MidiConverter.h>

#include <QByteArray>
#include <QList>
#include <QScopedPointer>
#include <QWidget>

class MidiConfigPagePrivate;

// Interactive MIDI import configuration: text encoding, track selector with
// select-all, lyrics preview and import options. Hosted by the generic
// ProjectImportConfigDialog; the load session only consumes MidiUserInput.
class MidiConfigPage final : public QWidget, public IProjectConfigPage {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MidiConfigPage)
public:
    explicit MidiConfigPage(const QList<MidiImportTrackInfo> &trackInfoList,
                            QWidget *parent = nullptr);
    ~MidiConfigPage() override;

    QWidget *widget() override;

    void setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList);
    QList<MidiImportTrackInfo> trackInfoList() const;

    QList<int> selectedTracks() const;
    QByteArray selectedCodec() const;

    bool separateMidiChannels() const;
    bool importTempo() const;
    bool importTimeSignature() const;

    void detectCodec();

    void setSelectedCodec(const QByteArray &codec);
    void setSeparateMidiChannels(bool enabled);
    void setImportTempo(bool enabled);
    void setImportTimeSignature(bool enabled);

    MidiUserInput collectInput() const;

signals:
    void codecChanged(const QByteArray &codec);
    void selectedTracksChanged();
    void separateMidiChannelsChanged(bool enabled);
    void importTempoChanged(bool enabled);
    void importTimeSignatureChanged(bool enabled);

private:
    QScopedPointer<MidiConfigPagePrivate> d_ptr;
};

#endif // DS_EDITOR_LITE_MIDICONFIGPAGE_H
