#ifndef INFOLANEHEADERVIEW_H
#define INFOLANEHEADERVIEW_H

#include <QWidget>

class QLabel;

// Left-panel companion row of an InfoLaneView: shows the lane title, aligned
// with the lane on the right side of the splitter.
class InfoLaneHeaderView : public QWidget {
    Q_OBJECT

public:
    explicit InfoLaneHeaderView(QWidget *parent = nullptr);

    void setTitle(const QString &title);

private:
    QLabel *m_titleLabel = nullptr;
};

#endif // INFOLANEHEADERVIEW_H
