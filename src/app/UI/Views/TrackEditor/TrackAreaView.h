#ifndef TRACKAREAVIEW_H
#define TRACKAREAVIEW_H

#include <QWidget>

class DividerLine;
class QResizeEvent;
class QVBoxLayout;

class TrackAreaView final : public QWidget {
    Q_OBJECT

public:
    explicit TrackAreaView(QWidget *contentWidget, QWidget *parent = nullptr);

    void setContentWidget(QWidget *contentWidget);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_layout = nullptr;
    DividerLine *m_topDivider = nullptr;
};

#endif // TRACKAREAVIEW_H
