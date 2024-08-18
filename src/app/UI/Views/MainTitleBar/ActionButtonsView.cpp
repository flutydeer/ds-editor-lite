//
// Created by fluty on 2024/2/5.
//

#include "ActionButtonsView.h"

#include "Modules/History/HistoryManager.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QHBoxLayout>
#include <QPushButton>

ActionButtonsView::ActionButtonsView(QWidget *parent) : QWidget(parent) {
    auto dividerLine = new DividerLine(Qt::Vertical);
    dividerLine->setFixedHeight(m_contentHeight - 6);

    m_btnSave = new QPushButton;
    m_btnSave->setObjectName("btnSave");
    m_btnSave->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnSave->setIcon(icoSaveWhite);
    m_btnSave->setToolTip(tr("Save Project"));
    m_btnSave->installEventFilter(new ToolTipFilter(m_btnSave));
    connect(m_btnSave, &QPushButton::clicked, this, [=] { emit saveTriggered(); });

    m_btnUndo = new QPushButton;
    m_btnUndo->setObjectName("btnUndo");
    m_btnUndo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnUndo->setIcon(icoUndoWhite);
    m_btnUndo->setEnabled(false);
    // m_btnUndo->setShortcut(QKeyCombination(Qt::CTRL, Qt::Key_Z));
    m_btnUndo->setToolTip(tr("Undo"));
    m_btnUndo->installEventFilter(new ToolTipFilter(m_btnUndo));
    connect(m_btnUndo, &QPushButton::clicked, this, [=] { emit undoTriggered(); });

    m_btnRedo = new QPushButton;
    m_btnRedo->setObjectName("btnRedo");
    m_btnRedo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnRedo->setIcon(icoRedoWhite);
    m_btnRedo->setEnabled(false);
    // m_btnRedo->setShortcut(QKeyCombination(Qt::CTRL, Qt::Key_Y));
    m_btnRedo->setToolTip(tr("Redo"));
    m_btnRedo->installEventFilter(new ToolTipFilter(m_btnRedo));
    connect(m_btnRedo, &QPushButton::clicked, this, [=] { emit redoTriggered(); });

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(4, 0, 6, 0);
    mainLayout->setSpacing(4);
    setLayout(mainLayout);
    setContentsMargins({});
    mainLayout->addWidget(dividerLine);
    mainLayout->addWidget(m_btnSave);
    mainLayout->addWidget(m_btnUndo);
    mainLayout->addWidget(m_btnRedo);

    connect(this, &ActionButtonsView::undoTriggered, historyManager, &HistoryManager::undo);
    connect(this, &ActionButtonsView::redoTriggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            &ActionButtonsView::onUndoRedoChanged);
}

void ActionButtonsView::onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                          const QString &redoActionName) {
    m_btnUndo->setEnabled(canUndo);
    m_btnUndo->setToolTip(tr("Undo") + " " + undoActionName);
    m_btnRedo->setEnabled(canRedo);
    m_btnRedo->setToolTip(tr("Redo") + " " + redoActionName);
}