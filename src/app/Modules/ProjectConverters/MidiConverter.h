#ifndef DS_EDITOR_LITE_MIDICONVERTER_H
#define DS_EDITOR_LITE_MIDICONVERTER_H

#include "IProjectConverter.h"

using ImportMode = IProjectConverter::ImportMode;

class MidiConverter final : public IProjectConverter {
public:
    static int midiImportHandler();
    bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) override;
    bool save(const QString &path, AppModel *model, QString &errMsg) override;
};



#endif // DS_EDITOR_LITE_MIDICONVERTER_H
