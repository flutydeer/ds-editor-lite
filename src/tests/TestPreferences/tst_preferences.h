#pragma once

#include <QObject>

class PreferencesTests final : public QObject {
    Q_OBJECT

private slots:
    void inferenceSingerSessionCacheSettings();
    void inferencePlaybackLookaheadPersistence();
    void automationOptionDefaults();
    void automationOptionRoundTrip();
    void automationOptionInvalidValuesUseSafeDefaults();
    void automationOptionControlLevelConversion_data();
    void automationOptionControlLevelConversion();
    void automationOptionControlPortInput_data();
    void automationOptionControlPortInput();
    void automationOptionInvalidPermissionAndLevelCannotBeEnabled();
    void automationOptionStableGeneratedControlPort();
    void automationOptionMcpClientConfigurations();
    void uiLanguageEffectiveLanguage_data();
    void uiLanguageEffectiveLanguage();
    void uiLanguageNumberFormattingPreservesDecimalSeparator();
    void uiLanguagePreferencePersistence();
    void uiLanguageEquivalentPreferenceDoesNotNotify();
};
