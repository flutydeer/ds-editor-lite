#include "LibreSVIPFormatHandler.h"

#include "Controller/DocumentWorkflow/LibreSVIPLoadSession.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"

#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
    constexpr auto kSettingsKey = "libresvip/executablePath";
}

ProjectFormatDescriptor LibreSVIPFormatHandler::descriptor() const {
    ProjectFormatDescriptor result;
    result.id = QStringLiteral("libresvip");
    result.displayName = QStringLiteral("Multiple formats (via LibreSVIP)");
    // Input formats supported by libresvip-cli, excluding mid/midi and dspx
    // which have native handlers (registry resolution prefers earlier
    // registrations). The generic .xml suffix of MusicXML is omitted on
    // purpose to avoid hijacking arbitrary XML files.
    result.extensions = {
        QStringLiteral("acep"),     QStringLiteral("acet"),       QStringLiteral("aisp"),
        QStringLiteral("ccs"),      QStringLiteral("ds"),         QStringLiteral("dv"),
        QStringLiteral("sk"),       QStringLiteral("json"),       QStringLiteral("mtp"),
        QStringLiteral("musicxml"), QStringLiteral("mxl"),        QStringLiteral("nn"),
        QStringLiteral("ppsf"),     QStringLiteral("ps_project"), QStringLiteral("s5p"),
        QStringLiteral("svip"),     QStringLiteral("svip3"),      QStringLiteral("svp"),
        QStringLiteral("tlp"),      QStringLiteral("tlpx"),       QStringLiteral("tsmsln"),
        QStringLiteral("tssln"),    QStringLiteral("ufdata"),     QStringLiteral("ust"),
        QStringLiteral("ustx"),     QStringLiteral("vfp"),        QStringLiteral("vog"),
        QStringLiteral("vpr"),      QStringLiteral("vshp"),       QStringLiteral("vspx"),
        QStringLiteral("vsq"),      QStringLiteral("vsqx"),       QStringLiteral("vvproj"),
        QStringLiteral("vxf"),      QStringLiteral("xvsq"),       QStringLiteral("y77")};
    result.canImport = true;
    return result;
}

bool LibreSVIPFormatHandler::probe(const QByteArray &header) const {
    // Bridged formats cannot be identified by a shared file header; extension
    // resolution in the registry is authoritative.
    Q_UNUSED(header)
    return true;
}

IProjectLoadSession *LibreSVIPFormatHandler::createSession(const ProjectLoadRequest &request,
                                                           IDocumentWorkflowUi * /*ui*/,
                                                           QObject *parent) {
    if (request.purpose != ProjectLoadPurpose::Import)
        return nullptr;
    return new LibreSVIPLoadSession(this, request.filePath, request.requestId, parent);
}

IProjectConfigPage *LibreSVIPFormatHandler::createConfigPage(QWidget *parent) {
    return new DspxConfigPage({}, parent);
}

QString LibreSVIPFormatHandler::executablePath() {
    QSettings settings;
    const auto configured = settings.value(QString::fromLatin1(kSettingsKey)).toString();
    if (!configured.isEmpty() && QFileInfo::exists(configured))
        return configured;
    return QStandardPaths::findExecutable(QStringLiteral("libresvip-cli"));
}

void LibreSVIPFormatHandler::setExecutablePath(const QString &path) {
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), path);
}
