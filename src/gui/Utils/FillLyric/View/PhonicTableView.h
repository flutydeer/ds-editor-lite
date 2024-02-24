#ifndef DS_EDITOR_LITE_PHONICTABLEVIEW_H
#define DS_EDITOR_LITE_PHONICTABLEVIEW_H

#include <QTableView>

namespace FillLyric {

    class PhonicTableView : public QTableView {
        Q_OBJECT
    public:
        explicit PhonicTableView(QWidget *parent = nullptr);
        ~PhonicTableView() override;

    Q_SIGNALS:
        void sizeChanged() const;

    protected:
        void resizeEvent(QResizeEvent *event) override;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICTABLEVIEW_H
