//
// Created by FlutyDeer on 2025/8/1.
//

#include "PackageItemDelegate.h"

#include "synthrt/Core/PackageRef.h"

#include <QPainter>

PackageItemDelegate::PackageItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {
    locale = QLocale::system().name().toStdString();
}

void PackageItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    painter->save();

    // Draw background
    // QStyleOptionViewItem opt = option;
    // initStyleOption(&opt, index);
    // opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

    // Load data
    const auto &package = index.data(Qt::UserRole).value<srt::PackageRef>();
    const auto id = QString::fromStdString(package.id());
    const auto vendor = QString::fromStdString(package.vendor().text(locale));
    const auto version = "v" + QString::fromStdString(package.version().toString());

    // Calculate layout
    QRectF contentRect = option.rect.adjusted(m_paddingLeft, m_paddingTop, -m_paddingRight,
                                              -m_paddingBottom);

    // Calculate title text rect
    auto titleFont = option.font;
    titleFont.setPixelSize(m_titlePixelSize);
    const QFontMetrics idMetrics(titleFont);
    qreal idTextWidth = idMetrics.horizontalAdvance(id);
    auto idTextAscent = idMetrics.ascent();
    QPointF idTextPos = {contentRect.left(), contentRect.top() + idMetrics.ascent()};

    auto descTextY = contentRect.top() + idMetrics.height() + m_spacing;

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

    // Draw title text
    painter->setPen(m_colorTitle);
    painter->setFont(titleFont);
    painter->drawText(idTextPos, id);

    // Draw vendor text
    painter->setPen(m_colorDesc);
    painter->setFont(vendorFont);
    painter->drawText(vendorTextPos, elidedVendorText);

    // Draw version text
    painter->setPen(m_colorDesc);
    painter->setFont(versionFont);
    painter->drawText(versionTextPos, version);

    painter->restore();
}

QSize PackageItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
    // return QStyledItemDelegate::sizeHint(option, index);

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

QColor PackageItemDelegate::colorTitle() const {
    return m_colorTitle;
}

void PackageItemDelegate::setColorTitle(const QColor &color) {
    m_colorTitle = color;
}

QColor PackageItemDelegate::colorDesc() const {
    return m_colorDesc;
}

void PackageItemDelegate::setColorDesc(const QColor &color) {
    m_colorDesc = color;
}

int PackageItemDelegate::titlePixelSize() const {
    return m_titlePixelSize;
}

void PackageItemDelegate::setTitlePixelSize(int pixelSize) {
    m_titlePixelSize = pixelSize;
}

int PackageItemDelegate::descPixelSize() const {
    return m_descPixelSize;
}

void PackageItemDelegate::setDescPixelSize(int pixelSize) {
    m_descPixelSize = pixelSize;
}

double PackageItemDelegate::paddingLeft() const {
    return m_paddingLeft;
}

void PackageItemDelegate::setPaddingLeft(double padding) {
    m_paddingLeft = padding;
}

double PackageItemDelegate::paddingRight() const {
    return m_paddingRight;
}

void PackageItemDelegate::setPaddingRight(double padding) {
    m_paddingRight = padding;
}

double PackageItemDelegate::paddingTop() const {
    return m_paddingTop;
}

void PackageItemDelegate::setPaddingTop(double padding) {
    m_paddingTop = padding;
}

double PackageItemDelegate::paddingBottom() const {
    return m_paddingBottom;
}

void PackageItemDelegate::setPaddingBottom(double padding) {
    m_paddingBottom = padding;
}

double PackageItemDelegate::spacing() const {
    return m_spacing;
}

void PackageItemDelegate::setSpacing(double spacing) {
    m_spacing = spacing;
}