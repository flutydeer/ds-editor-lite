#include "tst_application_services.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

void ApplicationServicesTests::packageVersionAndPathProjection() {
    ApplicationHarness harness;
    harness.packages = {
        {.id = QStringLiteral("voice.package"),
         .version = QVersionNumber(1, 0),
         .path = QStringLiteral("private/voice-v1")},
        {.id = QStringLiteral("voice.package"),
         .version = QVersionNumber(2, 0),
         .path = QStringLiteral("allowed/voice-v2")},
    };
    auto &runtime = harness.core();

    const auto projection = [](const QString &path) -> std::optional<QString> {
        if (path.startsWith(QStringLiteral("allowed")))
            return path;
        return std::nullopt;
    };
    const auto packages = runtime.packages().getInstalledPackages(projection);
    QVERIFY2(
        (packages && packages.get().size() == 2 && packages.get().first().path.isEmpty() &&
         !packages.get().last().path.isEmpty()),
        qPrintable(QStringLiteral("packages.list must omit paths outside allowed read roots")));
    const auto described =
        runtime.packages().describePackage(QStringLiteral("voice.package"), projection);
    QVERIFY2((described && described.get().version == QVersionNumber(2, 0)),
             qPrintable(QStringLiteral(
                 "packages.describe must deterministically select the newest version")));
    const auto describedVersion = runtime.packages().describePackage(
        QStringLiteral("voice.package"), QStringLiteral("1.0"), projection);
    QVERIFY2((describedVersion && describedVersion.get().version == QVersionNumber(1, 0) &&
              describedVersion.get().path.isEmpty()),
             qPrintable(
                 QStringLiteral("packages.describe must select and project an explicit version")));
    const auto invalidVersion = runtime.packages().describePackage(
        QStringLiteral("voice.package"), QStringLiteral("1..0"), projection);
    QVERIFY2(
        (!invalidVersion &&
         invalidVersion.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         invalidVersion.getError().fieldPath == QStringLiteral("version")),
        qPrintable(QStringLiteral("packages.describe must reject malformed explicit versions")));
}

void ApplicationServicesTests::packageRefreshPreviewAndResultProjection() {
    ApplicationHarness harness;
    harness.packages = {
        {.id = QStringLiteral("voice.package"),
         .version = QVersionNumber(1, 0),
         .path = QStringLiteral("private/voice-v1")},
        {.id = QStringLiteral("voice.package"),
         .version = QVersionNumber(2, 0),
         .path = QStringLiteral("allowed/voice-v2")},
    };
    auto &runtime = harness.core();

    const auto projection = [](const QString &path) -> std::optional<QString> {
        return path.startsWith(QStringLiteral("allowed")) ? std::optional(path) : std::nullopt;
    };
    bool previewCompleted = false;
    const auto preview = runtime.packages().refreshPackages(
        applicationContext(true),
        [&](const Automation::AutomationResult<Automation::PackageRefreshResultDto> &result) {
            previewCompleted = result && result.get().packages == 2 && result.get().added.isEmpty();
        },
        projection);
    QVERIFY2((preview && previewCompleted && harness.refreshStarts == 0),
             qPrintable(QStringLiteral(
                 "packages.refresh validate_only must not start a filesystem scan")));

    bool refreshCompleted = false;
    const auto refreshed = runtime.packages().refreshPackages(
        applicationContext(),
        [&](const Automation::AutomationResult<Automation::PackageRefreshResultDto> &result) {
            refreshCompleted =
                result && result.get().added == QStringList{QStringLiteral("new.package@1.0")} &&
                result.get().failures.first().path.isEmpty();
        },
        projection);
    QVERIFY2(
        (refreshed && refreshCompleted && harness.refreshStarts == 1),
        qPrintable(QStringLiteral("packages.refresh must start once and project failure paths")));
}

void ApplicationServicesTests::packageSearchPaths() {
    ApplicationHarness harness;
    auto &runtime = harness.core();

    const QStringList input{
        QStringLiteral(" packages/../voices/主声库 "),
        QStringLiteral("voices/主声库"),
        QStringLiteral(" "),
        QStringLiteral("voices/secondary"),
    };
    const auto preview = runtime.settings().setPackageSearchPaths(applicationContext(true), input);
    const auto committed = runtime.settings().setPackageSearchPaths(applicationContext(), input);
    const auto noOp = runtime.settings().setPackageSearchPaths(
        applicationContext(),
        {QStringLiteral("voices/主声库"), QStringLiteral("voices/secondary")});
    QVERIFY2(
        (preview && preview.get().validatedOnly && preview.get().changed && committed &&
         committed.get().changed && noOp && !noOp.get().changed &&
         harness.settings.general.packageSearchPaths ==
             QStringList{QStringLiteral("voices/主声库"), QStringLiteral("voices/secondary")}),
        qPrintable(QStringLiteral("package paths must normalize, deduplicate and preserve order")));

    harness.settingsApplySucceeds = false;
    const auto failed = runtime.settings().setPackageSearchPaths(
        applicationContext(), {QStringLiteral("voices/failure")});
    QVERIFY2(
        (!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
         failed.getError().operationId == Automation::OperationIds::packages::set_search_paths),
        qPrintable(QStringLiteral("package path persistence failure must be reported")));
    QVERIFY2((harness.settings.general.packageSearchPaths ==
              QStringList{QStringLiteral("voices/主声库"), QStringLiteral("voices/secondary")}),
             qPrintable(QStringLiteral("failed package path write must preserve stored paths")));
}

void ApplicationServicesTests::packages() {
    ApplicationHarness harness;
    auto &runtime = harness.core();

    const auto installed = runtime.packages().getInstalledPackages();
    const auto expected = harness.packages;
    harness.packages.clear();
    QVERIFY2(
        (installed && installed.get() == expected && installed.get().first().singers.size() == 1),
        qPrintable(
            QStringLiteral("installed package query must return a detached typed snapshot")));

    const auto validated =
        runtime.packages().validatePackage(QStringLiteral("packages/candidate.dspk"));
    QVERIFY2((validated && !validated.get().hasErrors && validated.get().items.size() == 1 &&
              harness.packageValidationCalls == 1 &&
              harness.lastValidatedPackagePath == QStringLiteral("packages/candidate.dspk")),
             qPrintable(QStringLiteral("package validation must preserve the backend report")));

    const auto empty = runtime.packages().validatePackage(QStringLiteral(" "));
    harness.packageValidationSucceeds = false;
    const auto backendFailure =
        runtime.packages().validatePackage(QStringLiteral("packages/broken.dspk"));
    QVERIFY2((!empty && empty.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              empty.getError().operationId == Automation::OperationIds::packages::validate),
             qPrintable(QStringLiteral("empty package path must be rejected")));
    QVERIFY2(
        (!backendFailure &&
         backendFailure.getError().code == Automation::AutomationErrorCode::IoError &&
         backendFailure.getError().operationId == Automation::OperationIds::packages::validate),
        qPrintable(QStringLiteral("package validator failure must retain its error")));

    const auto version = runtime.documentVersion();
    const auto preview = runtime.packages().resolveDocumentVoices(commandContext(runtime, true));
    const auto applied = runtime.packages().resolveDocumentVoices(commandContext(runtime));
    QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed && applied &&
              applied.get().changed && harness.packageResolvePreviewCalls == 1 &&
              harness.packageResolveApplyCalls == 1 && runtime.documentVersion() == version),
             qPrintable(
                 QStringLiteral("voice resolution must preview/apply without document revision")));

    harness.packageResolveCount = 0;
    const auto noOp = runtime.packages().resolveDocumentVoices(commandContext(runtime));
    QVERIFY2((noOp && !noOp.get().changed && harness.packageResolveApplyCalls == 2 &&
              runtime.documentVersion() == version),
             qPrintable(QStringLiteral("zero resolved voices must be a successful no-op")));

    auto stale = commandContext(runtime);
    ++stale.expected.revision;
    const auto staleResult = runtime.packages().resolveDocumentVoices(stale);
    auto replaced = stale;
    replaced.expected.documentId = Automation::DocumentId::create();
    const auto replacedResult = runtime.packages().resolveDocumentVoices(replaced);
    QVERIFY2((!staleResult &&
              staleResult.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
              staleResult.getError().operationId ==
                  Automation::OperationIds::packages::resolve_document_voices),
             qPrintable(QStringLiteral("voice resolution must check revision before its service")));
    QVERIFY2((!replacedResult &&
              replacedResult.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
              replacedResult.getError().operationId ==
                  Automation::OperationIds::packages::resolve_document_voices),
             qPrintable(QStringLiteral("voice resolution must check document before revision")));
    QVERIFY2((harness.packageResolveApplyCalls == 2),
             qPrintable(QStringLiteral("routing failures must not call voice resolution")));

    auto keyed = commandContext(runtime);
    keyed.idempotencyKey = QStringLiteral("package-resolve-key");
    const auto unsupportedKey = runtime.packages().resolveDocumentVoices(keyed);
    QVERIFY2(
        (!unsupportedKey &&
         unsupportedKey.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         unsupportedKey.getError().operationId ==
             Automation::OperationIds::packages::resolve_document_voices),
        qPrintable(QStringLiteral("cache-only voice resolution must reject idempotency keys")));
}
