//
// Created by fluty on 2026/5/26.
//

#ifndef CONTROLGROUP_H
#define CONTROLGROUP_H

#include <QWidget>

class ControlGroup : public QWidget {
    Q_OBJECT

public:
    explicit ControlGroup(QWidget *parent = nullptr);

protected:
    void childEvent(QChildEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void scheduleUpdate();
    void updateGroupPositions();
    bool m_updatePending = false;
};

#endif // CONTROLGROUP_H
