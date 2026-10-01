#include <lite/GUI/Controls/DragHandle.h>

#include <lite/GUI/Controls/TouchClaimFilter.h>

#include <QPointF>

DragHandle::DragHandle(QWidget *parent) : IconLabel(parent) {
    setObjectName(QStringLiteral("dragHandle"));
    // Not every list wants the same grip size (the FillLyric rows and the
    // speaker mixer differ), so the size stays the caller's call.
    setIcon(QStringLiteral(":/svg/icons/re_order_dots_vertical_16_regular.svg"));
    setCursor(Qt::SizeAllCursor);
    // Installed once and gated by m_dragEnabled, so toggling dragging never has
    // to uninstall an event filter - an uninstall would also make the claim
    // re-entrant with the sweep that installed it.
    TouchClaimFilter::install(this, [this](const QPointF &) { return m_dragEnabled; });
}

void DragHandle::setDragEnabled(const bool enabled) {
    if (m_dragEnabled == enabled)
        return;
    m_dragEnabled = enabled;
    setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor);
    setEnabled(enabled);
}

bool DragHandle::isDragEnabled() const {
    return m_dragEnabled;
}
