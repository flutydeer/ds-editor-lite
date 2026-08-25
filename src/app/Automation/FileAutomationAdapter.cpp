#include "FileAutomationAdapter.h"
#include "OperationIds.h"

#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/LibreSVIPFormatHandler.h"
#include "Modules/ProjectFormats/ProjectFormatRegistry.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/ProjectConverters/LibreSVIPConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>

#include <utility>

namespace Automation {
    namespace {
        QJsonObject publicOptionProperties(const QString &operationId,
                                           const QStringList &propertyNames) {
            QJsonObject properties;
            const auto *contract = AutomationWire::findPublicTool(operationId);
            if (!contract)
                return AutomationWire::JsonSchema::object();
            const auto source = contract->inputSchema.value(QStringLiteral("properties"))
                                    .toObject()
                                    .value(QStringLiteral("options"))
                                    .toObject()
                                    .value(QStringLiteral("properties"))
                                    .toObject();
            for (const auto &name : propertyNames) {
                if (source.contains(name))
                    properties.insert(name, source.value(name));
            }
            return AutomationWire::JsonSchema::object(properties);
        }

        QJsonObject publicNestedOptions(const QString &operationId) {
            const auto *contract = AutomationWire::findPublicTool(operationId);
            if (!contract)
                return AutomationWire::JsonSchema::object();
            return contract->inputSchema.value(QStringLiteral("properties"))
                .toObject()
                .value(QStringLiteral("options"))
                .toObject();
        }

        QJsonObject optionBranch(const QString &operationId, const QJsonObject &options) {
            return AutomationWire::JsonSchema::object(
                {
                    {QStringLiteral("operation_id"),
                     AutomationWire::JsonSchema::constant(operationId)},
                    {QStringLiteral("options"), options},
                },
                {QStringLiteral("operation_id"), QStringLiteral("options")});
        }

        QJsonObject formatOptionSchema(const ProjectFormatDescriptor &descriptor) {
            QJsonArray branches;
            if (descriptor.canOpen) {
                const auto fields = descriptor.id == QStringLiteral("midi")
                                        ? QStringList{QStringLiteral("encoding")}
                                        : QStringList{};
                branches.append(optionBranch(
                    OperationIds::documents::open,
                    publicOptionProperties(OperationIds::documents::open, fields)));
            }
            if (descriptor.canImport) {
                QStringList fields{QStringLiteral("import_tempo"),
                                   QStringLiteral("import_time_signatures")};
                if (descriptor.id == QStringLiteral("midi"))
                    fields.prepend(QStringLiteral("encoding"));
                branches.append(optionBranch(
                    OperationIds::documents::import_document,
                    publicOptionProperties(OperationIds::documents::import_document, fields)));
            }
            if (descriptor.canExport && descriptor.id == QStringLiteral("midi")) {
                branches.append(optionBranch(
                    OperationIds::exports::midi::start,
                    publicNestedOptions(OperationIds::exports::midi::start)));
            }
            if (branches.isEmpty())
                return AutomationWire::JsonSchema::document(AutomationWire::JsonSchema::object());
            auto root = AutomationWire::JsonSchema::oneOf(branches);
            root.insert(QStringLiteral("type"), QStringLiteral("object"));
            return AutomationWire::JsonSchema::document(std::move(root));
        }
    }

    FileRuntimeServices createFileAutomationServices() {
        FileRuntimeServices services;
        services.listProjectFormats = [] {
            QList<ProjectFormatDto> result;
            for (const auto *handler : projectFormatRegistry->handlers()) {
                const auto descriptor = handler->descriptor();
                const auto *libreSvipHandler =
                    dynamic_cast<const LibreSVIPFormatHandler *>(handler);
                const auto available =
                    !libreSvipHandler || !LibreSVIPFormatHandler::executablePath().isEmpty();
                result.append({
                    .id = descriptor.id,
                    .displayName = descriptor.displayName,
                    .extensions = descriptor.extensions,
                    .canOpen = descriptor.canOpen,
                    .canImport = descriptor.canImport,
                    .canExport = descriptor.canExport,
                    .available = available,
                    .unavailableReason =
                        available ? QString()
                                  : QStringLiteral("libresvip-cli executable was not found"),
                    .optionSchema = formatOptionSchema(descriptor),
                });
            }
            return result;
        };
        services.convertLibreSvipToDspx = [](const QString &path) {
            const auto converted =
                LibreSVIPConverter::convertToDspx(LibreSVIPFormatHandler::executablePath(), path);
            if (converted.success())
                return AutomationResult<QByteArray>(converted.dspxData);
            AutomationError failure;
            failure.code = AutomationErrorCode::FormatUnsupported;
            failure.fieldPath = QStringLiteral("path");
            failure.message = converted.errorMessage;
            return AutomationResult<QByteArray>(std::move(failure));
        };
        services.exportMidi = [](AppModel *model, const QString &path,
                                 const MidiExportOptionsDto &options, QString &errorMessage) {
            if (!model) {
                errorMessage = QStringLiteral("AppModel is unavailable");
                return false;
            }
            MidiConverter converter;
            return converter.save(path, model, errorMessage,
                                  {.includeTempo = options.includeTempo,
                                   .includeTimeSignatures = options.includeTimeSignatures,
                                   .includeLyrics = options.includeLyrics});
        };
        return services;
    }

} // namespace Automation
