#include "Model/AppOptions/Options/GeneralOption.h"
#include "Utils/UiLanguageManager.h"

#include <QCoreApplication>
#include <QJsonObject>

namespace {

    bool expectEqual(const QString &actual, const QString &expected, const char *scenario) {
        if (actual == expected)
            return true;
        qCritical() << scenario << "expected" << expected << "but got" << actual;
        return false;
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool success = true;

    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        UiLanguageManager::System, QLocale(QLocale::Chinese, QLocale::China)),
                    UiLanguageManager::SimplifiedChinese, "Simplified Chinese system locale");
    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        UiLanguageManager::System, QLocale(QLocale::Chinese, QLocale::Taiwan)),
                    UiLanguageManager::SimplifiedChinese, "Traditional Chinese system locale");
    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        UiLanguageManager::System, QLocale(QLocale::Chinese, QLocale::HongKong)),
                    UiLanguageManager::SimplifiedChinese, "Hong Kong Chinese system locale");
    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        UiLanguageManager::System, QLocale(QLocale::German, QLocale::Germany)),
                    UiLanguageManager::English, "Unsupported system locale");
    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        UiLanguageManager::English, QLocale(QLocale::Chinese, QLocale::China)),
                    UiLanguageManager::English, "Explicit English preference");
    success &= expectEqual(
        UiLanguageManager::resolveEffectiveLanguageId(
            UiLanguageManager::SimplifiedChinese, QLocale(QLocale::English, QLocale::UnitedStates)),
        UiLanguageManager::SimplifiedChinese, "Explicit Chinese preference");
    success &=
        expectEqual(UiLanguageManager::resolveEffectiveLanguageId(
                        QStringLiteral("invalid"), QLocale(QLocale::Chinese, QLocale::China)),
                    UiLanguageManager::SimplifiedChinese, "Invalid preference fallback");

    GeneralOption option;
    option.load(QJsonObject{
        {QStringLiteral("uiLanguage"), QStringLiteral("invalid")}
    });
    success &=
        expectEqual(option.uiLanguage, UiLanguageManager::System, "Invalid persisted preference");

    option.uiLanguage = UiLanguageManager::English;
    success &= expectEqual(option.value().value(QStringLiteral("uiLanguage")).toString(),
                           UiLanguageManager::English, "Preference serialization");

    UiLanguageManager languageManager;
    int languageChangedCount = 0;
    QObject::connect(&languageManager, &UiLanguageManager::languageChanged,
                     [&languageChangedCount] { ++languageChangedCount; });
    languageManager.setPreference(UiLanguageManager::English);
    languageManager.setPreference(UiLanguageManager::English);
    success &= expectEqual(languageManager.preference(), UiLanguageManager::English,
                           "Equivalent preference update");
    success &= expectEqual(languageManager.effectiveLanguageId(), UiLanguageManager::English,
                           "Equivalent effective language");
    if (languageChangedCount != 0) {
        qCritical() << "Equivalent language preference emitted" << languageChangedCount
                    << "languageChanged signals";
        success = false;
    }

    return success ? 0 : 1;
}
