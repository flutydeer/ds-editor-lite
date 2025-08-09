//
// Created by fluty on 24-3-19.
//

#include "TaskView.h"

#include <QLabel>
#include <QVBoxLayout>

#include "UI/Controls/ProgressIndicator.h"

TaskView::TaskView(const TaskStatus &initialStatus, QWidget *parent)
    : QWidget(parent), UniqueObject(initialStatus.id) {
    m_lbTitle = new QLabel;
    m_lbMsg = new QLabel;
    m_progressBar = new ProgressIndicator;

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_lbTitle);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_lbMsg);
    setLayout(mainLayout);
    // setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    updateUi(initialStatus);
}

void TaskView::onTaskStatusChanged(const TaskStatus &status) const {
    updateUi(status);
}

void TaskView::updateUi(const TaskStatus &status) const {
    m_lbTitle->setText(status.title);
    m_lbMsg->setText(status.message);
    m_progressBar->setMaximum(status.maximum);
    m_progressBar->setMinimum(status.minimum);
    m_progressBar->setValue(status.progress);
    m_progressBar->setIndeterminate(status.isIndetermine);
    m_progressBar->setTaskStatus(status.runningStatus);
}