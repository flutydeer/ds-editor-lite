#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"
#include "Utils/UiLanguageManager.h"
#include <lite/ProjectModel/Voice/LanguageInfo.h>

#include <QEvent>
#include <QSet>
#include <QSignalBlocker>

namespace {
    QString displayName(const QString &id, const QString &packageName) {
        if (id.isEmpty())
            return LanguageComboBox::tr("Follow singer");
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

LanguageComboBox::LanguageComboBox(const QString &langKey, const WheelEventPolicy wheelEventPolicy,
                                   QWidget *parent)
    : ComboBox(wheelEventPolicy, parent) {
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
    m_languages = languages;

    QSet<QString> addedIds;
    for (const auto &language : languages) {
        const auto id = language.id().trimmed();
        if (id.isEmpty() || addedIds.contains(id))
            continue;
        addedIds.insert(id);
        addItem(displayName(id, language.displayName(UiLanguageManager::currentBcp47Candidates())),
                id);
        setItemData(count() - 1, language.name(), Qt::UserRole + 1);
    }

    if (count() == 0) {
        // no selectable languages (usually no singer) -> show "Follow singer" (auto/unspecified),
        // id is empty
        addItem(tr("Follow singer"), QString());
        setItemData(0, QString(), Qt::UserRole + 1);
    }

    auto selected = currentLanguage;
    if (findData(selected) < 0)
        selected = preferredLanguage;
    if (findData(selected) < 0)
        selected = itemData(0).toString();
    setCurrentIndex(findData(selected));
    adjustWidthToContent();
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
    const auto selected = setLanguages(languages, currentLanguage);
    m_languages.clear(); // the built-in list has no package-localized names
    return selected;
}

void LanguageComboBox::changeEvent(QEvent *event) {
    ComboBox::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        refreshDisplayNames();
}

void LanguageComboBox::refreshDisplayNames() {
    QSignalBlocker blocker(this);
    for (int index = 0; index < count(); ++index) {
        const auto id = itemData(index).toString();
        auto packageName = itemData(index, Qt::UserRole + 1).toString();
        for (const auto &language : m_languages) {
            if (language.id() == id) {
                packageName = language.displayName(UiLanguageManager::currentBcp47Candidates());
                break;
            }
        }
        setItemText(index, displayName(id, packageName));
    }
    adjustWidthToContent();
}

void LanguageComboBox::adjustWidthToContent() {
    int maxWidth = 0;
    const QFontMetrics fm(font());
    for (int i = 0; i < count(); ++i) {
        const int textWidth = fm.horizontalAdvance(itemText(i));
        maxWidth = qMax(maxWidth, textWidth);
    }

    // Account for: left text padding (~8), arrow button area (28),
    // right gap between text and arrow (~8), and frame border (~2+2)
    constexpr int kArrowArea = 28;
    constexpr int kPadding = 20;
    constexpr int kFrameBorder = 4;
    const int totalWidth = maxWidth + kArrowArea + kPadding + kFrameBorder;

    setMinimumWidth(totalWidth);
    updateGeometry();
}
