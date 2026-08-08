#ifndef DS_EDITOR_LITE_LIBRESVIPFORMATHANDLER_H
#define DS_EDITOR_LITE_LIBRESVIPFORMATHANDLER_H

#include "IProjectFormatHandler.h"

// Bridge-layer format handler: owns no parser of its own. Every extension in
// descriptor() is converted by the external libresvip-cli process to DSPX
// data at import time (see LibreSVIPConvertTask / LibreSVIPLoadSession).
// The track / timeline configuration page is reused from DSPX because the
// bridged parse result is an opendspx::Model.
class LibreSVIPFormatHandler final : public IProjectFormatHandler {
public:
    ProjectFormatDescriptor descriptor() const override;
    bool probe(const QByteArray &header) const override;
    IProjectLoadSession *createSession(const ProjectLoadRequest &request, IDocumentWorkflowUi *ui,
                                       QObject *parent) override;
    IProjectConfigPage *createConfigPage(QWidget *parent) override;

    // Path of the libresvip-cli executable: AppOptions general->libreSVIPPath
    // first, then PATH lookup. Empty string means the converter is not available.
    static QString executablePath();
    static void setExecutablePath(const QString &path);
};

#endif // DS_EDITOR_LITE_LIBRESVIPFORMATHANDLER_H
