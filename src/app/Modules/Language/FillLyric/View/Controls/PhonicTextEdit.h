#ifndef DS_EDITOR_LITE_PHONICTEXTEDIT_H
#define DS_EDITOR_LITE_PHONICTEXTEDIT_H

#include <QObject>
#include <QTextEdit>
#include <QWheelEvent>

namespace FillLyric {
    class PhonicTextEdit final : public QTextEdit {
        Q_OBJECT
    public:
        explicit PhonicTextEdit(QWidget *parent = nullptr);

    Q_SIGNALS:
        void fontChanged();

    protected:
        void wheelEvent(QWheelEvent *event) override;
    };
}

#endif // DS_EDITOR_LITE_PHONICTEXTEDIT_H
