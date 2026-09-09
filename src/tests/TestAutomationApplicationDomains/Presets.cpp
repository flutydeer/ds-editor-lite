#include "TestAutomationApplicationDomains.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

void TestAutomationApplicationDomains::speakerMixPresets() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto version = runtime.documentVersion();

    const auto initial = runtime.presets().getSpeakerMixPresets();
    QVERIFY2((initial && initial.get().isEmpty()),
             qPrintable(QStringLiteral("preset query must preserve an empty collection")));

    const auto preview = runtime.presets().saveSpeakerMixPreset(
        applicationContext(true), validPreset(QStringLiteral("主唱")));
    QVERIFY2(
        (preview && preview.get().id.isEmpty() && !preview.get().createdAt.isValid() &&
         !preview.get().updatedAt.isValid() && harness.presetWriteAttempts == 0 &&
         harness.presets.isEmpty()),
        qPrintable(QStringLiteral("preset preview must not allocate IDs, timestamps or storage")));

    const auto saved = runtime.presets().saveSpeakerMixPreset(applicationContext(),
                                                              validPreset(QStringLiteral("主唱")));
    QVERIFY2(
        (saved && !saved.get().id.isEmpty() && saved.get().createdAt.isValid() &&
         saved.get().updatedAt.isValid() &&
         harness.presets == QList<Automation::SpeakerMixPresetDto>{saved.get()} &&
         harness.presetWrites == 1 && runtime.documentVersion() == version),
        qPrintable(QStringLiteral("preset save must allocate metadata and persist atomically")));

    const auto listed = runtime.presets().getSpeakerMixPresets();
    harness.presets.first().name = QStringLiteral("mutated-after-query");
    QVERIFY2((listed && listed.get() == QList<Automation::SpeakerMixPresetDto>{saved.get()}),
             qPrintable(QStringLiteral("preset list must be an owned snapshot")));
    harness.presets = listed.get();

    auto update = saved.get();
    update.name = QStringLiteral("主唱更新");
    update.fixedWeights = {0.6};
    const auto updated = runtime.presets().saveSpeakerMixPreset(applicationContext(), update);
    QVERIFY2((updated && updated.get().id == saved.get().id &&
              updated.get().createdAt == saved.get().createdAt &&
              updated.get().name == QStringLiteral("主唱更新") &&
              harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()} &&
              harness.presetWrites == 2),
             qPrintable(QStringLiteral("preset update must preserve identity and creation time")));

    auto duplicate = validPreset(QStringLiteral("主唱更新"));
    const auto duplicateResult =
        runtime.presets().saveSpeakerMixPreset(applicationContext(), duplicate);
    QVERIFY2((!duplicateResult &&
              duplicateResult.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              duplicateResult.getError().operationId ==
                  Automation::OperationIds::speaker_mix_presets::save),
             qPrintable(QStringLiteral("same-singer duplicate preset name must be rejected")));

    auto emptyName = validPreset();
    emptyName.name.clear();
    auto mismatched = validPreset();
    mismatched.fixedWeights.removeLast();
    auto emptySpeaker = validPreset();
    emptySpeaker.sources.first().speakerId.clear();
    auto nonFinite = validPreset();
    nonFinite.fixedWeights.first() = std::numeric_limits<double>::quiet_NaN();
    const auto invalidName =
        runtime.presets().saveSpeakerMixPreset(applicationContext(), emptyName);
    const auto invalidSize =
        runtime.presets().saveSpeakerMixPreset(applicationContext(), mismatched);
    const auto invalidSpeaker =
        runtime.presets().saveSpeakerMixPreset(applicationContext(), emptySpeaker);
    const auto invalidWeight =
        runtime.presets().saveSpeakerMixPreset(applicationContext(), nonFinite);
    QVERIFY2(
        (!invalidName &&
         invalidName.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         invalidName.getError().operationId == Automation::OperationIds::speaker_mix_presets::save),
        qPrintable(QStringLiteral("preset identity fields must be required")));
    QVERIFY2(
        (!invalidSize &&
         invalidSize.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         invalidSize.getError().operationId == Automation::OperationIds::speaker_mix_presets::save),
        qPrintable(QStringLiteral("preset sources and weights must align")));
    QVERIFY2((!invalidSpeaker &&
              invalidSpeaker.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              invalidSpeaker.getError().operationId ==
                  Automation::OperationIds::speaker_mix_presets::save),
             qPrintable(QStringLiteral("preset speaker ID must be required")));
    QVERIFY2((!invalidWeight &&
              invalidWeight.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              invalidWeight.getError().operationId ==
                  Automation::OperationIds::speaker_mix_presets::save),
             qPrintable(QStringLiteral("preset weight must be finite")));

    auto failedUpdate = updated.get();
    failedUpdate.name = QStringLiteral("failed update");
    harness.presetApplySucceeds = false;
    const auto failed = runtime.presets().saveSpeakerMixPreset(applicationContext(), failedUpdate);
    harness.presetApplySucceeds = true;
    QVERIFY2((!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
              failed.getError().operationId == Automation::OperationIds::speaker_mix_presets::save),
             qPrintable(QStringLiteral("preset save failure must be reported")));
    QVERIFY2((harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()} &&
              harness.presetWrites == 2),
             qPrintable(QStringLiteral("failed preset save must preserve storage")));

    const auto deleteMissing =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), QStringLiteral("missing"));
    const auto deletePreview =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(true), updated.get().id);
    QVERIFY2(
        (deleteMissing && !deleteMissing.get().changed && deletePreview &&
         deletePreview.get().changed && deletePreview.get().validatedOnly &&
         harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()}),
        qPrintable(QStringLiteral("preset delete must no-op for missing and preview existing")));

    harness.presetApplySucceeds = false;
    const auto deleteFailed =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
    harness.presetApplySucceeds = true;
    const auto deleted =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
    const auto deletedAgain =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
    QVERIFY2((!deleteFailed &&
              deleteFailed.getError().code == Automation::AutomationErrorCode::IoError &&
              deleteFailed.getError().operationId ==
                  Automation::OperationIds::speaker_mix_presets::delete_preset),
             qPrintable(QStringLiteral("preset delete persistence failure must be stable")));
    QVERIFY2(
        (deleted && deleted.get().changed && deletedAgain && !deletedAgain.get().changed &&
         harness.presets.isEmpty() && runtime.documentVersion() == version),
        qPrintable(QStringLiteral("preset delete must commit once and preserve document version")));

    const auto invalidDelete =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), QStringLiteral(" "));
    QVERIFY2((!invalidDelete &&
              invalidDelete.getError().code == Automation::AutomationErrorCode::InvalidArgument),
             qPrintable(QStringLiteral("empty preset ID must be rejected")));
}
