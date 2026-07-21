//
// Created by fluty on 24-8-2.
//

#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"
#include "Modules/PackageManager/Models/LanguageInfo.h"

#include <QEvent>
#include <QSet>
#include <QSignalBlocker>

namespace {
    QString displayName(const QString &id, const QString &packageName) {
        if (id == QStringLiteral("cmn"))
            return LanguageComboBox::tr("Mandarin");
        if (id == QStringLiteral("eng"))
            return LanguageComboBox::tr("English");
        if (id == QStringLiteral("jpn"))
            return LanguageComboBox::tr("Japanese");
        if (id == QStringLiteral("yue"))
            return LanguageComboBox::tr("Cantonese");
        if (id == QStringLiteral("unknown"))
            return LanguageComboBox::tr("Unknown");
        if (!packageName.trimmed().isEmpty())
            return packageName.trimmed();
        return id;
    }
}

LanguageComboBox::LanguageComboBox(const QString &langKey, bool scrollWheelChangeSelection,
                                   QWidget *parent)
    : ComboBox(scrollWheelChangeSelection, parent) {
    setLanguageCodes(AppGlobal::languageNames, langKey);
    connect(this, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0)
            emit currentLanguageChanged(itemData(index).toString());
    });
}

QString LanguageComboBox::currentLanguage() const {
    return currentData().toString();
}

void LanguageComboBox::setCurrentLanguage(const QString &language) {
    setCurrentIndex(findData(language));
}

QString LanguageComboBox::setLanguages(const QList<LanguageInfo> &languages,
                                       const QString &currentLanguage,
                                       const QString &preferredLanguage) {
    QSignalBlocker blocker(this);
    clear();

    QSet<QString> addedIds;
    for (const auto &language : languages) {
        const auto id = language.id().trimmed();
        if (id.isEmpty() || addedIds.contains(id))
            continue;
        addedIds.insert(id);
        addItem(displayName(id, language.name()), id);
        setItemData(count() - 1, language.name(), Qt::UserRole + 1);
    }

    if (count() == 0) {
        addItem(displayName(QStringLiteral("unknown"), {}), QStringLiteral("unknown"));
        setItemData(0, QString(), Qt::UserRole + 1);
    }

    auto selected = currentLanguage;
    if (findData(selected) < 0)
        selected = preferredLanguage;
    if (findData(selected) < 0)
        selected = itemData(0).toString();
    setCurrentIndex(findData(selected));
    return selected;
}

QString LanguageComboBox::setLanguageCodes(const QStringList &languageCodes,
                                           const QString &currentLanguage,
                                           bool preserveUnknownCurrent) {
    QList<LanguageInfo> languages;
    QSet<QString> addedIds;
    for (const auto &languageCode : languageCodes) {
        const auto id = languageCode.trimmed();
        if (id.isEmpty() || addedIds.contains(id))
            continue;
        addedIds.insert(id);
        languages.emplace_back(id);
    }
    if (preserveUnknownCurrent && !currentLanguage.trimmed().isEmpty() &&
        !addedIds.contains(currentLanguage)) {
        languages.emplace_back(currentLanguage, currentLanguage);
    }
    return setLanguages(languages, currentLanguage);
}

void LanguageComboBox::changeEvent(QEvent *event) {
    ComboBox::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        refreshDisplayNames();
}

void LanguageComboBox::refreshDisplayNames() {
    QSignalBlocker blocker(this);
    for (int index = 0; index < count(); ++index) {
        setItemText(index, displayName(itemData(index).toString(),
                                       itemData(index, Qt::UserRole + 1).toString()));
    }
}
