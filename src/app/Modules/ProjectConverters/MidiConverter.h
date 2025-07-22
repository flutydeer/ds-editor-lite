#ifndef DS_EDITOR_LITE_MIDICONVERTER_H
#define DS_EDITOR_LITE_MIDICONVERTER_H

#include "IProjectConverter.h"
#include "Model/AppModel/TimeSignature.h"

using ImportMode = IProjectConverter::ImportMode;

class MidiConverter final : public IProjectConverter {
public:
    explicit MidiConverter(TimeSignature timeSignature, double tempo);
    static int midiImportHandler();
    bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) override;
    bool save(const QString &path, AppModel *model, QString &errMsg) override;

private:
    TimeSignature m_timeSignature;
    double m_tempo;
};

#endif // DS_EDITOR_LITE_MIDICONVERTER_H