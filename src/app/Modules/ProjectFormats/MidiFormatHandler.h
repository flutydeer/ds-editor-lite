#ifndef DS_EDITOR_LITE_MIDIFORMATHANDLER_H
#define DS_EDITOR_LITE_MIDIFORMATHANDLER_H

#include "IProjectFormatHandler.h"

class MidiFormatHandler final : public IProjectFormatHandler {
public:
    ProjectFormatDescriptor descriptor() const override;
    bool probe(const QByteArray &header) const override;
    IProjectLoadSession *createSession(const ProjectLoadRequest &request, QObject *parent) override;
};

#endif // DS_EDITOR_LITE_MIDIFORMATHANDLER_H
