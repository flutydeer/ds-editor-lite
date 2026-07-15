#include "UiLanguageManager.h"

#include <QApplication>
#include <QLibraryInfo>

UiLanguageManager *UiLanguageManager::s_instance = nullptr;

UiLanguageManager::UiLanguageManager(QObject *parent) : QObject(parent) {
    Q_ASSERT(!s_instance);
    s_instance = this;
}

UiLanguageManager::~UiLanguageManager() {
    removeTranslators();
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
    removeTranslators();

    if (requestedLanguage == SimplifiedChinese && !loadChineseTranslators()) {
        qWarning() << "Failed to load the complete zh_CN translation set; falling back to English";
        removeTranslators();
        m_effectiveLanguageId = English;
    } else {
        m_effectiveLanguageId = requestedLanguage;
    }

    if (previousLanguage != m_effectiveLanguageId)
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

void UiLanguageManager::removeTranslators() {
    if (!qApp)
        return;
    qApp->removeTranslator(&m_appTranslator);
    qApp->removeTranslator(&m_qtTranslator);
    qApp->removeTranslator(&m_qtBaseTranslator);
}

bool UiLanguageManager::loadChineseTranslators() {
    const QLocale locale(SimplifiedChinese);
    const auto qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    const bool qtBaseLoaded = m_qtBaseTranslator.load(locale, QStringLiteral("qtbase"),
                                                      QStringLiteral("_"), qtTranslationsPath);
    const bool qtLoaded =
        m_qtTranslator.load(locale, QStringLiteral("qt"), QStringLiteral("_"), qtTranslationsPath);
    const bool appLoaded = m_appTranslator.load(QStringLiteral(":/i18n/translation_zh_CN.qm"));

    const bool qtTranslationsLoaded = qtBaseLoaded || qtLoaded;
    if (!qtTranslationsLoaded)
        qWarning() << "Failed to load Qt zh_CN translations from" << qtTranslationsPath;
    if (!appLoaded)
        qWarning() << "Failed to load application translation :/i18n/translation_zh_CN.qm";
    if (!qtTranslationsLoaded || !appLoaded)
        return false;

    if (qtBaseLoaded)
        qApp->installTranslator(&m_qtBaseTranslator);
    if (qtLoaded)
        qApp->installTranslator(&m_qtTranslator);
    qApp->installTranslator(&m_appTranslator);
    return true;
}
