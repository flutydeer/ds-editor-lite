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

    // Disabling convention: Qt::UserRole - 1 is the slot in which Qt 6 QStandardItemModel stores
    // the item flags, and writing 0 there clears ItemIsEnabled/ItemIsSelectable — the item then
    // shows greyed out in the popup, the mouse cannot click it and the arrow keys skip it. A value
    // in that slot means "disabled", and every item disabled by this change is written through the
    // same place, markUnavailableItem().
    constexpr int kUnavailableFlagsRole = Qt::UserRole - 1;
    // The original reason text is stored in the Qt::UserRole + 2 slot of the item data; an empty
    // string means "no original text, use the fallback wording".
    constexpr int kUnavailableReasonRole = Qt::UserRole + 2;

    // The original reason comes from the loader/engine and is copied verbatim, not translated: only
    // it knows why it refused, the same convention as SingerMenuUnavailablePackages. The fallback
    // wording is used only when there is no original text (for example "the singer does not declare
    // this language").
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
            // When the selection becomes (or stops being) a disabled language, the widget tooltip
            // has to follow or be restored
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

    // A key being present means disabled, and its value is the original reason for that language
    // (may be empty = use the fallback wording). Whether an item is disabled is decided by
    // `unavailable` alone, and the reason table only supplies the original text, so a call site
    // that gives reasons only for languages "the engine reports an error for" still disables every
    // item it must.
    QHash<QString, QString> disabled;
    for (const auto &code : unavailable) {
        const auto id = code.trimmed();
        if (!id.isEmpty())
            disabled.insert(id, unavailableReasons.value(id).trimmed());
    }

    // Whether "Follow singer" is inserted depends on the number of selectable items, not on the
    // number of items: when every language is unavailable the fallback is needed as well.
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
        // A declared language may also be unavailable (its G2P cannot be converted): it is disabled
        // in the same way, only its localized name and the engine reason are kept
        if (disabled.contains(id)) {
            markUnavailableItem(count() - 1, disabled.value(id));
        } else {
            ++selectable;
        }
    }

    // Undeclared disabled languages are listed with their generic name; `unknown` is a sentinel and
    // never listed as a selectable item. Both disabling cases and the reason convention are in the
    // header.
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

    auto selected = currentLanguage;
    if (findData(selected) < 0)
        selected = preferredLanguage;
    if (findData(selected) < 0)
        selected = itemData(0).toString();
    setCurrentIndex(findData(selected));
    adjustWidthToContent();
    // When the selected item is a disabled one, the closed combo box itself has to give the reason
    // as well (the tooltip of that row inside the popup is a separate matter)
    refreshCurrentToolTip();
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

void LanguageComboBox::markUnavailableItem(const int index, const QString &reason) {
    setItemData(index, 0, kUnavailableFlagsRole);
    setItemData(index, reason.trimmed(), kUnavailableReasonRole);
    setItemData(index, unavailableToolTipText(reason), Qt::ToolTipRole);
}

void LanguageComboBox::refreshCurrentToolTip() {
    const int index = currentIndex();
    const auto reason = index >= 0 && itemData(index, kUnavailableFlagsRole).isValid()
                            ? itemData(index, kUnavailableReasonRole).toString()
                            : QString();
    if (reason.trimmed().isEmpty()) {
        // The current language is available: hand the tooltip back to the caller (for example a
        // description like "Clip Default Language"), instead of holding on to it
        if (!m_reasonToolTip.isEmpty()) {
            setToolTip(m_baseToolTip);
            m_reasonToolTip.clear();
        }
        return;
    }

    const auto text = unavailableToolTipText(reason);
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
    // a) the host knows it, but the singer does not declare it: list it as before (otherwise it
    // would be silently replaced by another language), and the fallback wording is the only
    // reason available
    for (const auto &code : AppGlobal::languageNames) {
        if (!declared.contains(code))
            unavailable.append(code);
    }

    // For an unresolved singer (a fallback-constructed dspx, a voicebank that was not matched) the
    // engine has no answer, and the query would only produce noise
    if (singerInfo.resolutionState() != ResolutionState::Resolved)
        return unavailable;

    // b) declared, but the engine says it cannot be converted: disable only this one, and copy the
    // engine reason verbatim (an empty original leaves the fallback wording)
    const auto identifier = singerInfo.identifier();
    QSet<QString> queried;
    for (const auto &language : singerInfo.languages()) {
        const auto id = language.id().trimmed();
        if (id.isEmpty() || queried.contains(id))
            continue;
        queried.insert(id);
        const auto reason = SynthrtEngine::instance().languageUnavailableReason(identifier, id);
        if (reason.isEmpty())
            continue;
        unavailable.append(id);
        reasons.insert(id, reason);
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
        // The tooltip is item data, so setting the text again on a language change does not touch
        // it; the fallback wording, however, has to follow the translation, so it is recomputed for
        // the current language
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
