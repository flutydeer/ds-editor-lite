#include "PackageAutomationFacade.h"
#include "CommandCommitter.h"
#include "DocumentSession.h"
#include "OperationIds.h"


#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/NoteWordCascade.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <algorithm>

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Package services are unavailable");
            return error;
        }

        AutomationError packageNotFound(const QString &packageId) {
            AutomationError error;
            error.code = AutomationErrorCode::NotFound;
            error.fieldPath = QStringLiteral("package_id");
            error.message = QStringLiteral("Package was not found: %1").arg(packageId);
            return error;
        }

        void projectPackagePath(PackageDto &package, const PackagePathProjection &projection) {
            if (!projection)
                return;
            if (const auto projected = projection(package.path))
                package.path = *projected;
            else
                package.path.clear();
        }

        void projectRefreshPaths(PackageRefreshResultDto &result,
                                 const PackagePathProjection &projection) {
            if (!projection)
                return;
            for (auto &failure : result.failures) {
                if (const auto projected = projection(failure.path))
                    failure.path = *projected;
                else
                    failure.path.clear();
            }
        }
    }

    PackageAutomationFacade::PackageAutomationFacade(AutomationDispatcher &dispatcher,
                                                     PackageRuntimeServices services)
        : m_dispatcher(dispatcher), m_services(std::move(services)) {
    }

    AutomationResult<QList<PackageDto>> PackageAutomationFacade::getInstalledPackages() {
        return getInstalledPackages({});
    }

    AutomationResult<QList<PackageDto>>
        PackageAutomationFacade::getInstalledPackages(PackagePathProjection pathProjection) {
        return m_dispatcher.dispatchApplicationQuery<QList<PackageDto>>(
            OperationIds::packages::list, [this, pathProjection = std::move(pathProjection)] {
                if (!m_services.installedPackages)
                    return AutomationResult<QList<PackageDto>>(unavailable());
                auto packages = m_services.installedPackages();
                for (auto &package : packages)
                    projectPackagePath(package, pathProjection);
                return AutomationResult<QList<PackageDto>>(std::move(packages));
            });
    }

    AutomationResult<PackageDto>
        PackageAutomationFacade::describePackage(const QString &packageId,
                                                 PackagePathProjection pathProjection) {
        return describePackage(packageId, {}, std::move(pathProjection));
    }

    AutomationResult<PackageDto>
        PackageAutomationFacade::describePackage(const QString &packageId, const QString &version,
                                                 PackagePathProjection pathProjection) {
        return m_dispatcher.dispatchApplicationQuery<PackageDto>(
            OperationIds::packages::list,
            [this, packageId = packageId.trimmed(), version = version.trimmed(),
             pathProjection = std::move(pathProjection)]() mutable {
                if (packageId.isEmpty()) {
                    return AutomationResult<PackageDto>(AutomationError::invalidArgument(
                        QStringLiteral("package_id"), QStringLiteral("Package ID is empty")));
                }
                if (!m_services.installedPackages)
                    return AutomationResult<PackageDto>(unavailable());
                QVersionNumber requestedVersion;
                if (!version.isEmpty()) {
                    qsizetype suffixIndex = 0;
                    requestedVersion = QVersionNumber::fromString(version, &suffixIndex);
                    if (requestedVersion.isNull() || suffixIndex != version.size()) {
                        return AutomationResult<PackageDto>(AutomationError::invalidArgument(
                            QStringLiteral("version"),
                            QStringLiteral("Package version is invalid")));
                    }
                }
                const auto packages = m_services.installedPackages();
                const PackageDto *selected = nullptr;
                for (const auto &package : packages) {
                    if (package.id != packageId ||
                        (!version.isEmpty() && package.version != requestedVersion))
                        continue;
                    if (!selected ||
                        QVersionNumber::compare(package.version, selected->version) > 0)
                        selected = &package;
                }
                if (!selected)
                    return AutomationResult<PackageDto>(packageNotFound(packageId));
                auto result = *selected;
                projectPackagePath(result, pathProjection);
                return AutomationResult<PackageDto>(std::move(result));
            });
    }

    AutomationResult<AutomationUnit> PackageAutomationFacade::refreshPackages(
        const ApplicationCommandContext &context, PackageRefreshCompletion completion,
        PackagePathProjection pathProjection, PackageRefreshCommitGate commitGate) {
        return m_dispatcher.dispatchApplicationCommand<AutomationUnit>(
            OperationIds::packages::refresh, context,
            [this, completion = std::move(completion), pathProjection = std::move(pathProjection),
             commitGate = std::move(commitGate)](const bool validateOnly) mutable {
                if (!completion) {
                    return AutomationResult<AutomationUnit>(AutomationError::invalidArgument(
                        QStringLiteral("completion"),
                        QStringLiteral("Package refresh completion callback is missing")));
                }
                if (!m_services.installedPackages)
                    return AutomationResult<AutomationUnit>(unavailable());
                if (validateOnly) {
                    completion(PackageRefreshResultDto{
                        .packages = static_cast<int>(m_services.installedPackages().size()),
                    });
                    return AutomationResult<AutomationUnit>(AutomationUnit{});
                }
                if (!m_services.refreshPackages)
                    return AutomationResult<AutomationUnit>(unavailable());
                return m_services.refreshPackages(
                    std::move(commitGate),
                    [completion = std::move(completion),
                     pathProjection = std::move(pathProjection)](
                        AutomationResult<PackageRefreshResultDto> result) mutable {
                        if (result) {
                            auto projected = result.get();
                            projectRefreshPaths(projected, pathProjection);
                            completion(std::move(projected));
                        } else {
                            completion(result.getError());
                        }
                    });
            });
    }

    AutomationResult<PackageValidationReportDto>
        PackageAutomationFacade::validatePackage(const QString &path) {
        return m_dispatcher.dispatchApplicationQuery<PackageValidationReportDto>(
            OperationIds::packages::validate, [this, path] {
                if (path.trimmed().isEmpty()) {
                    return AutomationResult<PackageValidationReportDto>(
                        AutomationError::invalidArgument(QStringLiteral("path"),
                                                         QStringLiteral("Package path is empty")));
                }
                if (!m_services.validatePackage)
                    return AutomationResult<PackageValidationReportDto>(unavailable());
                return m_services.validatePackage(path);
            });
    }

    AutomationResult<MutationResult>
        PackageAutomationFacade::resolveDocumentVoices(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::packages::resolve_document_voices, context,
            [this](DocumentSession &session, const bool validateOnly) {
                if (!m_services.resolveDocumentVoices)
                    return AutomationResult<MutationResult>(unavailable());
                // A resolved route changes the language unspecified notes resolve
                // to. The word data of those notes moves with it, so the change is
                // applied as one document edit with its own revision and undo step
                // instead of being written behind the document's back.
                QList<SingingClip *> clips;
                QList<QString> fallbacksBefore;
                for (auto *track : session.model()->tracks()) {
                    for (auto *clip : track->clips()) {
                        if (clip->clipType() != IClip::Singing)
                            continue;
                        auto *singing = static_cast<SingingClip *>(clip);
                        clips.append(singing);
                        fallbacksBefore.append(singing->fallbackLanguage());
                    }
                }
                const int resolvedCount =
                    m_services.resolveDocumentVoices(session.model(), !validateOnly);
                CommandCommitter committer;
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        committer.preview(session, resolvedCount > 0));
                // The resolved voice context is a derived state, not an undoable
                // edit, so the word data it invalidates is applied as the same
                // derived state: the two sides never separate, and the revision
                // still advances for clients holding the old one.
                QList<QPair<SingingClip *, QList<Note *>>> cascades;
                QList<ObjectRef> affected;
                for (qsizetype i = 0; i < clips.count(); ++i) {
                    const auto plans = NoteWordCascade::planFallbackChange(
                        clips.at(i)->notes().toList(), fallbacksBefore.at(i),
                        clips.at(i)->fallbackLanguage());
                    if (plans.isEmpty())
                        continue;
                    QList<Note *> notes;
                    for (const auto &plan : plans) {
                        notes.append(plan.note);
                        affected.append({ObjectKind::Note, plan.note->id()});
                    }
                    cascades.append({clips.at(i), notes});
                }
                const bool changed = resolvedCount > 0 || !cascades.isEmpty();
                return AutomationResult<MutationResult>(committer.commitStateChange(
                    session, changed,
                    [&cascades, &clips, &fallbacksBefore] {
                        for (const auto &[clip, notes] : cascades) {
                            NoteWordCascade::applyFallbackChange(
                                notes, fallbacksBefore.at(clips.indexOf(clip)),
                                clip->fallbackLanguage());
                            clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange,
                                                    notes);
                        }
                    },
                    affected));
            });
    }

} // namespace Automation
