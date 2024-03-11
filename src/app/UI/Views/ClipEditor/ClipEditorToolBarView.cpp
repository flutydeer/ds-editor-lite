//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorToolBarView.h"

#include <QHBoxLayout>
#include <QButtonGroup>
#include <QPushButton>

#include "UI/Controls/ToolTipFilter.h"

ClipEditorToolBarView::ClipEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setObjectName("ClipEditorToolBarView");

    m_elClipName = new EditLabel;
    m_elClipName->setFixedWidth(128);
    m_elClipName->setFixedHeight(m_contentHeight);
    m_elClipName->setEnabled(false);
    m_elClipName->setText("");
    m_elClipName->setStyleSheet(
        "QLabel { color: #F0F0F0; background: #10FFFFFF; border-radius: 4px; }"
        "QLabel:hover { background: #1AFFFFFF; }"
        "QLineEdit { border-radius: 4px; }");
    connect(m_elClipName, &EditLabel::editCompleted, this,
            [=](const QString &text) { emit clipNameChanged(text); });

    m_btnArrow = new QPushButton;
    m_btnArrow->setObjectName("btnArrow");
    m_btnArrow->setCheckable(true);
    m_btnArrow->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnArrow->setChecked(true);
    m_btnArrow->setToolTip(tr("Select"));
    m_btnArrow->installEventFilter(new ToolTipFilter(m_btnArrow, 500, false, true));

    m_btnNotePencil = new QPushButton;
    m_btnNotePencil->setObjectName("btnNotePencil");
    m_btnNotePencil->setCheckable(true);
    m_btnNotePencil->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnNotePencil->setToolTip(tr("Draw Note"));
    m_btnNotePencil->installEventFilter(new ToolTipFilter(m_btnNotePencil, 500, false, true));

    m_btnPitchAnchor = new QPushButton;
    m_btnPitchAnchor->setObjectName("btnPitchAnchor");
    m_btnPitchAnchor->setCheckable(true);
    m_btnPitchAnchor->setIcon(icoPitchAnchorWhite);
    m_btnPitchAnchor->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnPitchAnchor->setToolTip(tr("Pitch Anchor"));
    m_btnPitchAnchor->installEventFilter(new ToolTipFilter(m_btnPitchAnchor, 500, false, true));

    m_btnPitchPencil = new QPushButton;
    m_btnPitchPencil->setObjectName("btnPitchPencil");
    m_btnPitchPencil->setCheckable(true);
    m_btnPitchPencil->setFixedSize(m_contentHeight, m_contentHeight);
    m_btnPitchPencil->setToolTip(tr("Draw Pitch"));
    m_btnPitchPencil->installEventFilter(new ToolTipFilter(m_btnPitchPencil, 500, false, true));

    auto buttonGroup = new QButtonGroup;
    buttonGroup->setExclusive(true);
    buttonGroup->addButton(m_btnArrow);
    buttonGroup->addButton(m_btnNotePencil);
    buttonGroup->addButton(m_btnPitchAnchor);
    buttonGroup->addButton(m_btnPitchPencil);
    connect(buttonGroup, &QButtonGroup::buttonToggled, this,
            [=](QAbstractButton *button, bool checked) {
                if (!checked)
                    return;

                qDebug() << button->objectName();
                if (button == m_btnArrow)
                    emit editModeChanged(ClipEditorGlobal::Select);
                else if (button == m_btnNotePencil)
                    emit editModeChanged(ClipEditorGlobal::DrawNote);
                else if (button == m_btnPitchPencil)
                    emit editModeChanged(ClipEditorGlobal::DrawPitch);
                else if (button == m_btnPitchAnchor)
                    emit editModeChanged(ClipEditorGlobal::EditPitchAnchor);
            });

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_elClipName);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_btnArrow);
    mainLayout->addWidget(m_btnNotePencil);
    mainLayout->addWidget(m_btnPitchAnchor);
    mainLayout->addWidget(m_btnPitchPencil);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    setLayout(mainLayout);
    setStyleSheet(
        "QPushButton {padding: 0px; border: none; background: none; border-radius: 6px; }"
        "QPushButton:checked { background-color: #9BBAFF; }"
        "QPushButton:hover { background: #1AFFFFFF; }"
        "QPushButton:pressed { background: #10FFFFFF; }"
        "QPushButton#btnArrow { icon: url(:svg/icons/cursor_24_filled_white.svg) }"
        "QPushButton#btnArrow:checked { icon: url(:svg/icons/cursor_24_filled.svg)} "
        "QPushButton#btnArrow:checked:hover { background-color: #9BBAFF;} "

        "QPushButton#btnNotePencil { icon: url(:svg/icons/edit_24_filled_white.svg) }"
        "QPushButton#btnNotePencil:checked { icon: url(:svg/icons/edit_24_filled.svg)} "
        "QPushButton#btnNotePencil:checked:hover { background-color: #9BBAFF;} "

        "QPushButton#btnPitchAnchor { icon: url(:svg/icons/pitch_anchor_24_filled_white.svg) }"
        "QPushButton#btnPitchAnchor:checked { icon: url(:svg/icons/pitch_anchor_24_filled.svg)} "
        "QPushButton#btnPitchAnchor:checked:hover { background-color: #9BBAFF;} "

        "QPushButton#btnPitchPencil { icon: url(:svg/icons/pitch_edit_24_filled_white.svg) }"
        "QPushButton#btnPitchPencil:checked { icon: url(:svg/icons/pitch_edit_24_filled.svg)} "
        "QPushButton#btnPitchPencil:checked:hover { background-color: #9BBAFF;} "

        "QComboBox { color: #F0F0F0; border: none; background: #10FFFFFF; border-radius: 6px; }"
        "QComboBox:hover { background: #1AFFFFFF; }"
        "QComboBox:pressed { background: #10FFFFFF; }");
    setFixedHeight(m_contentHeight + 12);
}
void ClipEditorToolBarView::setClipName(const QString &name) {
    m_elClipName->setText(name);
}
void ClipEditorToolBarView::setClipPropertyEditorEnabled(bool on) {
    if (on) {
        m_elClipName->setEnabled(true);
        // TODO: mute and gain
    } else {
        m_elClipName->setEnabled(false);
    }
}
void ClipEditorToolBarView::setPianoRollEditToolsEnabled(bool on) {
    // TODO: enable / disable arrow, pencil, ...
}