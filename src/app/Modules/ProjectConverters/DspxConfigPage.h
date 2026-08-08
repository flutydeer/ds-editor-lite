#ifndef DS_EDITOR_LITE_DSPXCONFIGPAGE_H
#define DS_EDITOR_LITE_DSPXCONFIGPAGE_H

#include "Modules/ProjectFormats/IProjectConfigPage.h"
#include "Modules/ProjectFormats/UserInput.h"
#include <lite/ProjectConverters/MidiConverter.h>

#include <QList>
#include <QScopedPointer>
#include <QWidget>

class DspxConfigPagePrivate;

// Interactive DSPX import configuration: track selector and timeline options.
// Reuses MidiImportTrackInfo as the generic track presentation record
// (name / rangeText as track type / noteCount). Hosted by the generic
// ProjectImportConfigDialog; the load session only consumes DspxUserInput.
class DspxConfigPage final : public QWidget, public IProjectConfigPage {
    Q_OBJECT
    Q_DECLARE_PRIVATE(DspxConfigPage)
public:
    explicit DspxConfigPage(const QList<MidiImportTrackInfo> &trackInfoList,
                            QWidget *parent = nullptr);
    ~DspxConfigPage() override;

    QWidget *widget() override;

    void setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList);

    QList<int> selectedTracks() const;
    bool importTempo() const;
    bool importTimeSignature() const;

    void setImportTempo(bool enabled);
    void setImportTimeSignature(bool enabled);

    DspxUserInput collectInput() const;

signals:
    void selectedTracksChanged();
    void importTempoChanged(bool enabled);
    void importTimeSignatureChanged(bool enabled);

private:
    QScopedPointer<DspxConfigPagePrivate> d_ptr;
};

#endif // DS_EDITOR_LITE_DSPXCONFIGPAGE_H
