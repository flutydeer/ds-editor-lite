#ifndef DS_EDITOR_LITE_PHONICTEXTEDIT_H
#define DS_EDITOR_LITE_PHONICTEXTEDIT_H

#include <QObject>
#include <QTextEdit>
#include <QWheelEvent>

namespace FillLyric {
    class PhonicTextEdit : public QTextEdit {
        Q_OBJECT
    public:
        explicit PhonicTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {
            auto f = font();
            f.setPointSizeF(11);
            setFont(f);
        }

    protected:
        void wheelEvent(QWheelEvent *event) override {
            if (event->modifiers() & Qt::ControlModifier) {
                auto fontSizeDelta = event->angleDelta().y() / 120.0;
                QFont font = this->font();
                auto newSize = font.pointSizeF() + fontSizeDelta;
                if (newSize > 0) {
                    font.setPointSizeF(newSize);
                    this->setFont(font);
                }
                event->accept();
                return;
            }
            QTextEdit::wheelEvent(event);
        }
    };
}

#endif // DS_EDITOR_LITE_PHONICTEXTEDIT_H
