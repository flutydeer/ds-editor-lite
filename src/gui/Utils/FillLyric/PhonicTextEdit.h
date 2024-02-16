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
        }

    protected:
        void wheelEvent(QWheelEvent *event) override {
            if (event->modifiers() & Qt::ControlModifier) {
                int fontSizeDelta = event->angleDelta().y() / 120;
                QFont font = this->font();
                int newSize = font.pointSize() + fontSizeDelta;
                if (newSize > 0) {
                    font.setPointSize(newSize);
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
