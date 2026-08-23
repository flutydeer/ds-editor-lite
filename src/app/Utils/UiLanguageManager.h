#ifndef UILANGUAGEMANAGER_H
#define UILANGUAGEMANAGER_H

#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QTranslator>

class UiLanguageManager final : public QObject {
    Q_OBJECT

public:
    static inline const QString System = QStringLiteral("system");
    static inline const QString English = QStringLiteral("en_US");
    static inline const QString SimplifiedChinese = QStringLiteral("zh_CN");

    explicit UiLanguageManager(QObject *parent = nullptr);
    ~UiLanguageManager() override;

    static UiLanguageManager *instance();
    static QString normalizePreference(const QString &preference);
    static QString resolveEffectiveLanguageId(const QString &preference,
                                              const QLocale &systemLocale);

    void setPreference(const QString &preference);
    [[nodiscard]] QString preference() const;
    [[nodiscard]] QString effectiveLanguageId() const;
    [[nodiscard]] QLocale effectiveLocale() const;
    [[nodiscard]] QString effectiveBcp47Name() const;
    [[nodiscard]] QStringList effectiveBcp47Candidates() const;
    static QString currentBcp47Name();
    static QStringList currentBcp47Candidates();

signals:
    void languageChanged(const QString &effectiveLanguageId);

private:
    void removeTranslators();
    bool loadChineseTranslators();

    static UiLanguageManager *s_instance;

    QString m_preference = System;
    QString m_effectiveLanguageId = English;
    QTranslator m_qtBaseTranslator;
    QTranslator m_qtTranslator;
    QTranslator m_appTranslator;
};

#endif // UILANGUAGEMANAGER_H
