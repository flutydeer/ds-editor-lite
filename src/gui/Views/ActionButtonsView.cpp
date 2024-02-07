//
// Created by fluty on 2024/2/5.
//

#include "ActionButtonsView.h"

#include <QHBoxLayout>

ActionButtonsView::ActionButtonsView(QWidget *parent) {
    m_btnSave = new QPushButton;
    m_btnSave->setObjectName("btnSave");
    m_btnSave->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnSave->setIcon(icoSaveWhite);
    connect(m_btnSave, &QPushButton::clicked, this, [=] { emit saveTriggered(); });

    m_btnUndo = new QPushButton;
    m_btnUndo->setObjectName("btnUndo");
    m_btnUndo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnUndo->setIcon(icoUndoWhite);
    m_btnUndo->setEnabled(false);
    m_btnUndo->setShortcut(QKeyCombination(Qt::CTRL, Qt::Key_Z));
    connect(m_btnUndo, &QPushButton::clicked, this, [=] { emit undoTriggered(); });

    m_btnRedo = new QPushButton;
    m_btnRedo->setObjectName("btnRedo");
    m_btnRedo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnRedo->setIcon(icoRedoWhite);
    m_btnRedo->setEnabled(false);
    m_btnUndo->setShortcut(QKeyCombination(Qt::CTRL, Qt::Key_Y));
    connect(m_btnRedo, &QPushButton::clicked, this, [=] { emit redoTriggered(); });

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);
    setLayout(mainLayout);
    setContentsMargins({});
    mainLayout->addWidget(m_btnSave);
    mainLayout->addWidget(m_btnUndo);
    mainLayout->addWidget(m_btnRedo);

    setStyleSheet("QPushButton {padding: 0px; border: none; background: none; border-radius: 6px}"
                  "QPushButton:hover { background: #1AFFFFFF; }"
                  "QPushButton:pressed { background: #10FFFFFF; }");
}
void ActionButtonsView::onUndoRedoChanged(bool canUndo, bool canRedo) {
    m_btnUndo->setEnabled(canUndo);
    m_btnRedo->setEnabled(canRedo);
}