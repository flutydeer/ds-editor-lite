#pragma once

#include <QObject>

class AutomationProtocolTests final : public QObject {
    Q_OBJECT

private slots:
    void canonicalJsonEncoding();
    void jsonSchema();
    void exposurePolicy();
    void mcpProtocol();
    void cursorRoundTrip_data();
    void cursorRoundTrip();
    void invalidCursorOffset_data();
    void invalidCursorOffset();
    void changedScopeInvalidatesCursor_data();
    void changedScopeInvalidatesCursor();
    void malformedCursor_data();
    void malformedCursor();
    void routing_data();
    void routing();
    void batchImportRouting_data();
    void batchImportRouting();
    void batchImportPlanRevalidation();
    void fillLyricsOptions_data();
    void fillLyricsOptions();
    void fillLyricsUnavailableLanguage();
    void parameterQueryBoundsSamplesAndPreservesAnchors();
    void parameterReplacementDecodesDrawAndAnchorCurves();
    void phonemeNamesUseTheEffectiveLanguageAndResetOffsets_data();
    void phonemeNamesUseTheEffectiveLanguageAndResetOffsets();
    void rejectedInputs_data();
    void rejectedInputs();
    void sharedEditingScenario_data();
    void sharedEditingScenario();
    void modernClientIdentity();
    void legacySessionLifecycle();
    void httpAdmission_data();
    void httpAdmission();
    void emptyOriginRejected();
    void transportMetadataRouting();
    void jsonAndRequestLimits();
    void handlerResponseLimits();
    void listenerLifecycle();
    void responseSurvivesShutdown_data();
    void responseSurvivesShutdown();
    void nativeRequestValidation();
    void nativeMcpRouteLifecycle();
    void nativeResponseLimit();
    void defaultRequestDeadline();
    void sharedAdmissionAndDeadline();
    void crossConnectionCancellation_data();
    void crossConnectionCancellation();
};
