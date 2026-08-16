#ifndef NOTELYRICTOOLTIP_H
#define NOTELYRICTOOLTIP_H

#include <QLabel>
#include <QRectF>

class NoteLyricToolTip final : public QLabel {
public:
    explicit NoteLyricToolTip(QWidget *parent = nullptr);

    void showAt(const QRectF &textRect, const QString &text, const QFont &font);
};

#endif // NOTELYRICTOOLTIP_H
