//
// Created by fluty on 2024/2/5.
//

#include "ActionButtonsView.h"

#include "Modules/History/HistoryManager.h"
#include "Controller/UndoRedoController.h"
#include "UI/Controls/ToolButton.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QPushButton>

ActionButtonsView::ActionButtonsView(QWidget *parent) : QWidget(parent) {
    m_btnSave = new ToolButton;
    m_btnSave->setObjectName("btnSave");
    m_btnSave->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnSave->setActionIcon(QStringLiteral(":/svg/icons/save_16_regular.svg"));
    m_btnSave->setToolTip(tr("Save Project"));
    m_btnSave->installEventFilter(new ToolTipFilter(m_btnSave));
    connect(m_btnSave, &QPushButton::clicked, this, [this] { emit saveTriggered(); });

    m_btnUndo = new ToolButton;
    m_btnUndo->setObjectName("btnUndo");
    m_btnUndo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnUndo->setActionIcon(QStringLiteral(":/svg/icons/arrow_undo_16_regular.svg"));
    m_btnUndo->setEnabled(false);
    m_btnUndo->setToolTip(tr("Undo"));
    m_btnUndo->installEventFilter(new ToolTipFilter(m_btnUndo));
    connect(m_btnUndo, &QPushButton::clicked, this, [this] { emit undoTriggered(); });

    m_btnRedo = new ToolButton;
    m_btnRedo->setObjectName("btnRedo");
    m_btnRedo->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnRedo->setActionIcon(QStringLiteral(":/svg/icons/arrow_redo_16_regular.svg"));
    m_btnRedo->setEnabled(false);
    m_btnRedo->setToolTip(tr("Redo"));
    m_btnRedo->installEventFilter(new ToolTipFilter(m_btnRedo));
    connect(m_btnRedo, &QPushButton::clicked, this, [this] { emit redoTriggered(); });

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 6, 0);
    mainLayout->setSpacing(4);
    setLayout(mainLayout);
    setContentsMargins({});
    mainLayout->addWidget(m_btnSave);
    mainLayout->addWidget(m_btnUndo);
    mainLayout->addWidget(m_btnRedo);

    connect(this, &ActionButtonsView::undoTriggered, undoRedoController,
            &UndoRedoController::requestUndo);
    connect(this, &ActionButtonsView::redoTriggered, undoRedoController,
            &UndoRedoController::requestRedo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            &ActionButtonsView::onUndoRedoChanged);
}

void ActionButtonsView::onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                          const QString &redoActionName) {
    m_undoActionName = undoActionName;
    m_redoActionName = redoActionName;
    m_btnUndo->setEnabled(canUndo);
    m_btnUndo->setToolTip(tr("Undo") + " " + undoActionName);
    m_btnRedo->setEnabled(canRedo);
    m_btnRedo->setToolTip(tr("Redo") + " " + redoActionName);
}

void ActionButtonsView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void ActionButtonsView::retranslateUi() {
    m_undoActionName = historyManager->undoActionName();
    m_redoActionName = historyManager->redoActionName();
    m_btnSave->setToolTip(tr("Save Project"));
    m_btnUndo->setToolTip(tr("Undo") + " " + m_undoActionName);
    m_btnRedo->setToolTip(tr("Redo") + " " + m_redoActionName);
}
