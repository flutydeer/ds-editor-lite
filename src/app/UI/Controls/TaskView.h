//
// Created by fluty on 24-3-19.
//

#ifndef TASKVIEW_H
#define TASKVIEW_H

#include <QWidget>

#include "Modules/Task/Task.h"

class ProgressIndicator;
class QLabel;

class TaskView : public QWidget, public UniqueObject {
    Q_OBJECT

public:
    explicit TaskView(const TaskStatus &initialStatus, QWidget *parent = nullptr);

public slots:
    void onTaskStatusChanged(const TaskStatus &status) const;

private:
    QLabel *m_lbTitle;
    QLabel *m_lbMsg;
    ProgressIndicator *m_progressBar;

    void updateUi(const TaskStatus &status) const;
};



#endif // TASKVIEW_H
