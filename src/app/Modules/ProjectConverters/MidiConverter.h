#ifndef DS_EDITOR_LITE_MIDICONVERTER_H
#define DS_EDITOR_LITE_MIDICONVERTER_H

#include "IProjectConverter.h"

using ImportMode = IProjectConverter::ImportMode;

class MidiConverter final : public IProjectConverter {
public:
    enum class LoadStatus { Success, Canceled, Failed };

    struct LoadOptions {
        bool importTempo = false;
        bool importTimeSignature = false;
    };

    explicit MidiConverter();
    bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) override;
    LoadStatus loadInteractive(const QString &path, AppModel *model, QString &errMsg,
                               ImportMode mode, LoadOptions &options);
    bool save(const QString &path, AppModel *model, QString &errMsg) override;
};

#endif // DS_EDITOR_LITE_MIDICONVERTER_H
