#include "LanguageComboBox.h"

#include "Global/AppGlobal.h"
#include "Utils/UiLanguageManager.h"
#include <lite/ProjectModel/Voice/LanguageInfo.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

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

    // QStandardItemModel stores item flags in Qt::UserRole - 1.
    constexpr int kUnavailableFlagsRole = Qt::UserRole - 1;
    constexpr int kUnavailableReasonRole = Qt::UserRole + 2;

    // Use translated fallback text only when the engine supplied no reason.
    QString unavailableToolTipText(const QString &reason) {
        const auto trimmed = reason.trimmed();
        if (!trimmed.isEmpty())
            return trimmed;
        return LanguageComboBox::tr("This singer does not serve this language");
    }
}

LanguageComboBox::LanguageComboBox(const QString &langKey, const WheelEventPolicy wheelEventPolicy,
                                   QWidget *parent)
    : ComboBox(wheelEventPolicy, parent) {
    setLanguageCodes(AppGlobal::languageNames, langKey);
    connect(this, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) {
            refreshCurrentToolTip();
            emit currentLanguageChanged(itemData(index).toString());
        }
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
                                       const QString &preferredLanguage,
                                       const QStringList &unavailable,
                                       const QHash<QString, QString> &unavailableReasons) {
    QSignalBlocker blocker(this);
    clear();
    m_languages = languages;

    // Availability comes from the unavailable set; reasons only supply tooltip text.
    QHash<QString, QString> disabled;
    for (const auto &code : unavailable) {
        const auto id = code.trimmed();
        if (!id.isEmpty())
            disabled.insert(id, unavailableReasons.value(id).trimmed());
    }

    // Offer 'Follow singer' when no language is selectable.
    int selectable = 0;
    QSet<QString> addedIds;
    for (const auto &language : languages) {
        const auto id = language.id().trimmed();
        if (id.isEmpty() || addedIds.contains(id))
            continue;
        addedIds.insert(id);
        addItem(displayName(id, language.displayName(UiLanguageManager::currentBcp47Candidates())),
                id);
        setItemData(count() - 1, language.name(), Qt::UserRole + 1);
        if (disabled.contains(id)) {
            markUnavailableItem(count() - 1, disabled.value(id));
        } else {
            ++selectable;
        }
    }

    // Keep undeclared languages visible, but never offer the unknown sentinel.
    for (const auto &code : unavailable) {
        const auto id = code.trimmed();
        if (id.isEmpty() || id == QStringLiteral("unknown") || addedIds.contains(id))
            continue;
        addedIds.insert(id);
        addItem(displayName(id, QString()), id);
        setItemData(count() - 1, QString(), Qt::UserRole + 1);
        markUnavailableItem(count() - 1, disabled.value(id));
    }

    if (selectable == 0) {
        // no selectable languages (usually no singer) -> show "Follow singer" (auto/unspecified),
        // id is empty
        insertItem(0, tr("Follow singer"), QString());
        setItemData(0, QString(), Qt::UserRole + 1);
    }

    auto selectedIndex = findData(currentLanguage);
    if (selectedIndex < 0) {
        selectedIndex = findData(preferredLanguage);
        if (selectedIndex < 0 || disabled.contains(itemData(selectedIndex).toString())) {
            selectedIndex = -1;
            for (int index = 0; index < count(); ++index) {
                if (!disabled.contains(itemData(index).toString())) {
                    selectedIndex = index;
                    break;
                }
            }
        }
    }
    setCurrentIndex(selectedIndex);
    adjustWidthToContent();
    refreshCurrentToolTip();
    return currentData().toString();
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

void LanguageComboBox::markUnavailableItem(const int index, const QString &reason) {
    setItemData(index, 0, kUnavailableFlagsRole);
    setItemData(index, reason.trimmed(), kUnavailableReasonRole);
    setItemData(index, unavailableToolTipText(reason), Qt::ToolTipRole);
}

void LanguageComboBox::refreshCurrentToolTip() {
    const int index = currentIndex();
    const bool unavailable = index >= 0 && itemData(index, kUnavailableFlagsRole).isValid();
    if (!unavailable) {
        if (!m_reasonToolTip.isEmpty()) {
            setToolTip(m_baseToolTip);
            m_reasonToolTip.clear();
        }
        return;
    }

    const auto text = unavailableToolTipText(itemData(index, kUnavailableReasonRole).toString());
    if (toolTip() != m_reasonToolTip) // caller just set its own tooltip: keep it, restore it later
        m_baseToolTip = toolTip();
    m_reasonToolTip = text;
    setToolTip(text);
}

QStringList LanguageComboBox::unavailableLanguages(const SingerInfo &singerInfo,
                                                   QHash<QString, QString> &reasons) {
    reasons.clear();

    QSet<QString> declared;
    for (const auto &language : singerInfo.languages()) {
        const auto id = language.id().trimmed();
        if (!id.isEmpty())
            declared.insert(id);
    }

    QStringList unavailable;
    // Keep undeclared languages visible so an explicit selection is not silently replaced.
    for (const auto &code : AppGlobal::languageNames) {
        if (!declared.contains(code))
            unavailable.append(code);
    }

    // Only resolved singers have language routes to probe.
    if (singerInfo.resolutionState() != ResolutionState::Resolved)
        return unavailable;

    const auto identifier = singerInfo.identifier();
    QSet<QString> queried;
    for (const auto &language : singerInfo.languages()) {
        const auto id = language.id().trimmed();
        if (id.isEmpty() || queried.contains(id))
            continue;
        queried.insert(id);
        if (SynthrtEngine::instance().canConvert(identifier, id))
            continue;
        unavailable.append(id);
        reasons.insert(id, SynthrtEngine::instance().languageUnavailableReason(identifier, id));
    }
    return unavailable;
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
        // Recompute translated fallback tooltips when the UI language changes.
        if (itemData(index, kUnavailableFlagsRole).isValid()) {
            const auto reason = itemData(index, kUnavailableReasonRole).toString();
            setItemData(index, unavailableToolTipText(reason), Qt::ToolTipRole);
        }
    }
    refreshCurrentToolTip();
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
