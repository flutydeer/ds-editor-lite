#include "EditDialog.h"

#include <QLineEdit>
#include <QHBoxLayout>
#include <qvalidator.h>

namespace FillLyric {
    EditDialog::EditDialog(const QString &lyric, const QRectF &rect, const QFont &font,
                           QWidget *parent)
        : QDialog(parent) {
        setObjectName("section-edit-popup");
        setWindowFlags(Qt::Popup);

        const auto lineEdit = new QLineEdit();
        lineEdit->setObjectName("tempo-text");
        lineEdit->setFont(font);
        lineEdit->setText(lyric);

        const QFontMetrics fm(lineEdit->font());
        const auto offset = fm.horizontalAdvance(" ") * 2;
        auto adjust = [=](const QString &text) {
            this->resize(fm.horizontalAdvance(text) + offset * 4, lineEdit->height()); //
        };
        connect(lineEdit, &QLineEdit::textChanged, this, adjust);
        connect(lineEdit, &QLineEdit::editingFinished, [=] { text = lineEdit->text(); });

        QDoubleValidator validator(0.0, std::numeric_limits<double>::max(), 3);
        validator.setNotation(QDoubleValidator::StandardNotation);

        const auto layout = new QHBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(lineEdit);

        this->setLayout(layout);
        this->adjustSize();
        this->move(
            parent->mapToGlobal(QPoint(rect.left(), rect.center().y() - this->height() / 2)));

        lineEdit->setFocus();
        lineEdit->setValidator(&validator);
        adjust(lineEdit->text());
    }

    bool EditDialog::event(QEvent *event) {
        switch (event->type()) {
            case QEvent::KeyPress:
            case QEvent::ShortcutOverride: {
                switch (dynamic_cast<QKeyEvent *>(event)->key()) {
                    case Qt::Key_Enter:
                    case Qt::Key_Return:
                        accept();
                        break;
                    case Qt::Key_Escape:
                        reject();
                        break;
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }
        return QDialog::event(event);
    }

} // FillLyric