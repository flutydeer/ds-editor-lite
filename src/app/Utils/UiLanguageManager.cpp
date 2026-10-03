#include "UiLanguageManager.h"

#include <QCoreApplication>

UiLanguageManager *UiLanguageManager::s_instance = nullptr;

UiLanguageManager::UiLanguageManager(QObject *parent) : QObject(parent) {
    Q_ASSERT(!s_instance);
    s_instance = this;
}

UiLanguageManager::~UiLanguageManager() {
    removeTranslator();
    s_instance = nullptr;
}

UiLanguageManager *UiLanguageManager::instance() {
    return s_instance;
}

QString UiLanguageManager::normalizePreference(const QString &preference) {
    if (preference == English || preference == SimplifiedChinese)
        return preference;
    return System;
}

QString UiLanguageManager::resolveEffectiveLanguageId(const QString &preference,
                                                      const QLocale &systemLocale) {
    const auto normalized = normalizePreference(preference);
    if (normalized != System)
        return normalized;
    return systemLocale.language() == QLocale::Chinese ? SimplifiedChinese : English;
}

void UiLanguageManager::setPreference(const QString &preference) {
    const auto normalized = normalizePreference(preference);
    const auto requestedLanguage = resolveEffectiveLanguageId(normalized, QLocale::system());
    const auto previousLanguage = m_effectiveLanguageId;

    m_preference = normalized;

    if (requestedLanguage == previousLanguage)
        return;

    if (requestedLanguage == SimplifiedChinese) {
        if (!loadChineseTranslator()) {
            qWarning()
                << "Failed to load the complete zh_CN translation set; falling back to English";
            removeTranslator();
            m_effectiveLanguageId = English;
            return;
        }
        m_effectiveLanguageId = SimplifiedChinese;
        QCoreApplication::installTranslator(&m_translator);
    } else {
        m_effectiveLanguageId = English;
        removeTranslator();
    }

    emit languageChanged(m_effectiveLanguageId);
}

QString UiLanguageManager::preference() const {
    return m_preference;
}

QString UiLanguageManager::effectiveLanguageId() const {
    return m_effectiveLanguageId;
}

QLocale UiLanguageManager::effectiveLocale() const {
    return QLocale(m_effectiveLanguageId);
}

QString UiLanguageManager::effectiveBcp47Name() const {
    // QLocale::bcp47Name() returns the SHORTEST tag ("zh"), which RFC 4647
    // lookup can only match against a bare "zh" key and misses "zh-Hans" /
    // "zh-CN". Prefer the first, most complete uiLanguages() candidate.
    return effectiveBcp47Candidates().isEmpty() ? QString() : effectiveBcp47Candidates().first();
}

QStringList UiLanguageManager::effectiveBcp47Candidates() const {
    return effectiveLocale().uiLanguages();
}

QString UiLanguageManager::currentBcp47Name() {
    return s_instance ? s_instance->effectiveBcp47Name() : QString();
}

QStringList UiLanguageManager::currentBcp47Candidates() {
    return s_instance ? s_instance->effectiveBcp47Candidates() : QStringList();
}

void UiLanguageManager::removeTranslator() {
    if (!QCoreApplication::instance())
        return;
    QCoreApplication::removeTranslator(&m_translator);
}

bool UiLanguageManager::loadChineseTranslator() {
    return m_translator.load(QStringLiteral(":/i18n/translation_zh_CN.qm"));
}
