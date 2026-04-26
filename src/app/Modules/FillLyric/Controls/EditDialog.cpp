#include "Modules/FillLyric/Controls/EditDialog.h"

#include <QHBoxLayout>
#include <QLineEdit>

namespace FillLyric
{
    EditDialog::EditDialog(const QString &lyric, const QRectF &rect, const QFont &font, QWidget *parent) :
        QDialog(parent) {
        setObjectName("section-edit-popup");
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);

        m_lineEdit = new QLineEdit();
        m_lineEdit->setObjectName("tempo-text");
        m_lineEdit->setFont(font);
        m_lineEdit->setText(lyric);

        const QFontMetrics fm(m_lineEdit->font());
        const auto offset = fm.horizontalAdvance(" ") * 2;
        auto adjust = [=](const QString &text)
        {
            this->resize(fm.horizontalAdvance(text) + offset * 4, m_lineEdit->height());
        };
        connect(m_lineEdit, &QLineEdit::textChanged, this, adjust);

        const auto layout = new QHBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_lineEdit);

        this->setLayout(layout);
        this->adjustSize();
        this->move(parent->mapToGlobal(QPoint(rect.left(), rect.center().y() - this->height() / 2)));

        m_lineEdit->setFocus();
        adjust(m_lineEdit->text());
    }

    QString EditDialog::text() const { return m_lineEdit->text(); }

    bool EditDialog::event(QEvent *event) {
        switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::ShortcutOverride:
            {
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

} // namespace FillLyric
