//
// Created by fluty on 24-8-19.
//

#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include <QWidget>

class PianoKeyboardView : public QWidget {
    Q_OBJECT

public:
    explicit PianoKeyboardView(QWidget *parent = nullptr);

public slots:
    void setKeyRange(double top, double bottom);

private:
    void paintEvent(QPaintEvent *) override;
    [[nodiscard]] double keyToY(double key) const;

    double m_top = 0;
    double m_bottom = 127;
};



#endif // PIANOKEYBOARDVIEW_H
