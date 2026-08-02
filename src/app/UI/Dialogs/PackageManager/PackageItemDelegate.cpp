#include "PackageItemDelegate.h"

#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/PackageManager/Models/PackageInfo.h>

#include <QPainter>

PackageItemDelegate::PackageItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {
}

QColor PackageItemDelegate::titleColor(const QStyleOptionViewItem &option,
                                       const bool selected) const {
    if (selected) {
        // Match the combobox popup's selected-item foreground (accent.default)
        const auto accent = ThemeManager::instance()->semanticColor(QStringLiteral("accent.default"));
        if (accent.isValid())
            return accent;
        return option.palette.color(QPalette::HighlightedText);
    }
    const auto primary = ThemeManager::instance()->semanticColor(QStringLiteral("text.primary"));
    if (primary.isValid())
        return primary;
    return option.palette.color(QPalette::Text);
}

QColor PackageItemDelegate::descColor(const QStyleOptionViewItem &option,
                                      const bool selected) const {
    if (selected) {
        // Match the combobox popup's selected-item foreground (accent.default)
        const auto accent = ThemeManager::instance()->semanticColor(QStringLiteral("accent.default"));
        if (accent.isValid())
            return accent;
        return option.palette.color(QPalette::HighlightedText);
    }
    const auto secondary = ThemeManager::instance()->semanticColor(QStringLiteral("text.secondary"));
    if (secondary.isValid())
        return secondary;
    return option.palette.color(QPalette::PlaceholderText);
}

void PackageItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    painter->save();

    // Draw background
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Load data
    const auto &package = index.data(Qt::UserRole).value<PackageInfo>();
    const auto id = package.id();
    const auto vendor = package.vendor();
    const auto version = "v" + package.version().toString();

    // Calculate layout
    QRectF contentRect = option.rect.adjusted(m_paddingLeft, m_paddingTop, -m_paddingRight,
                                              -m_paddingBottom);
    painter->setRenderHint(QPainter::Antialiasing);

    // Calculate title text rect
    auto titleFont = option.font;
    titleFont.setPixelSize(m_titlePixelSize);
    const QFontMetrics idMetrics(titleFont);
    qreal idTextWidth = idMetrics.horizontalAdvance(id);
    QPointF idTextPos = {contentRect.left(), contentRect.top() + idMetrics.ascent()};

    auto descTextY = contentRect.top() + idMetrics.ascent() + m_spacing;

    // Calculate version text rect
    auto versionFont = option.font;
    versionFont.setPixelSize(m_descPixelSize);
    const QFontMetrics versionMetrics(versionFont);
    qreal versionTextWidth = versionMetrics.horizontalAdvance(version);
    QPointF versionTextPos = {contentRect.right() - versionTextWidth,
                              descTextY + versionMetrics.ascent()};

    // Calculate vendor text rect
    auto vendorFont = option.font;
    vendorFont.setPixelSize(m_descPixelSize);
    const QFontMetrics vendorMetrics(vendorFont);
    auto vendorTextRectWidth = contentRect.width() - versionTextWidth - m_spacing;
    auto elidedVendorText = vendorMetrics.elidedText(vendor, Qt::ElideRight, vendorTextRectWidth);
    QPointF vendorTextPos = {contentRect.left(), descTextY + vendorMetrics.ascent()};

    const bool selected = opt.state & QStyle::State_Selected;
    const auto colorTitle = titleColor(option, selected);
    const auto colorDesc = descColor(option, selected);

    // Draw title text
    painter->setPen(colorTitle);
    painter->setFont(titleFont);
    painter->drawText(idTextPos, id);

    // Draw vendor text
    painter->setPen(colorDesc);
    painter->setFont(vendorFont);
    painter->drawText(vendorTextPos, elidedVendorText);

    // Draw version text
    painter->setPen(colorDesc);
    painter->setFont(versionFont);
    painter->drawText(versionTextPos, version);

    painter->restore();
}

QSize PackageItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {

    // Calculate title text height
    auto titleFont = option.font;
    titleFont.setPixelSize(m_titlePixelSize);
    const QFontMetrics idMetrics(titleFont);
    auto idTextHeight = idMetrics.height();

    // Calculate desc text height
    auto descFont = option.font;
    descFont.setPixelSize(m_descPixelSize);
    const QFontMetrics descMetrics(descFont);
    auto descTextHeight = descMetrics.height();

    auto height =
        qRound(m_paddingTop + idTextHeight + m_spacing + descTextHeight + m_paddingBottom);
    return {256, height};
}
