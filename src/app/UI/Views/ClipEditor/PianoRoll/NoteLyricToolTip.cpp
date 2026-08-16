#include "NoteLyricToolTip.h"

#include <algorithm>

NoteLyricToolTip::NoteLyricToolTip(QWidget *parent) : QLabel(parent) {
    setObjectName(QStringLiteral("noteLyricToolTip"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::NoFocus);
    setTextFormat(Qt::PlainText);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hide();
}

void NoteLyricToolTip::showAt(const QRectF &textRect, const QString &text, const QFont &font) {
    if (text.isEmpty() || !parentWidget()) {
        hide();
        return;
    }

    setFont(font);
    setText(text);
    ensurePolished();
    const auto hint = sizeHint().expandedTo(QSize(1, 1));
    const auto maximumX = std::max(0, parentWidget()->width() - hint.width());
    const auto maximumY = std::max(0, parentWidget()->height() - hint.height());
    const auto x = std::clamp(qRound(textRect.left()), 0, maximumX);
    const auto y = std::clamp(qRound(textRect.center().y() - hint.height() * 0.5), 0, maximumY);
    setGeometry(x, y, hint.width(), hint.height());
    raise();
    show();
}
