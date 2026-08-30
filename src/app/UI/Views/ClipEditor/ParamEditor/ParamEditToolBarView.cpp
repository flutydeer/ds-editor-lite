#include "ParamEditToolBarView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Utils/IconUtils.h>
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>

ParamEditToolBarView::ParamEditToolBarView(QWidget *parent) : QWidget(parent) {
    const auto createEditModeButton = [](const char *objectName, const QString &iconPath) {
        auto *button = new Button;
        button->setObjectName(QString::fromLatin1(objectName));
        button->setCheckable(true);
        button->setFixedSize(24, 24);
        button->setIconSize({16, 16});
        button->setIcon(
            IconUtils::createTintedSvgIcon(iconPath, {16, 16}, IconUtils::defaultActionPalette()));
        return button;
    };

    m_btnDraw =
        createEditModeButton("btnParamDraw", QStringLiteral(":/svg/icons/edit_24_filled.svg"));
    m_btnErase =
        createEditModeButton("btnParamErase", QStringLiteral(":/svg/icons/eraser_24_filled.svg"));
    m_btnBake =
        createEditModeButton("btnParamBake", QStringLiteral(":/svg/icons/brush_24_filled.svg"));
    m_btnBake->setEnabled(false);
    m_btnShape = createEditModeButton("btnParamShape",
                                      QStringLiteral(":/svg/icons/param_shape_24_filled.svg"));
    m_btnScale = createEditModeButton("btnParamScale",
                                      QStringLiteral(":/svg/icons/param_scale_24_filled.svg"));
    m_btnShape->setEnabled(false);
    m_btnScale->setEnabled(false);
    m_btnAnchor = createEditModeButton("btnParamAnchor",
                                       QStringLiteral(":/svg/icons/pitch_anchor_24_filled.svg"));

    m_editModeGroup = new QButtonGroup(this);
    m_editModeGroup->setExclusive(true);
    m_editModeGroup->addButton(m_btnDraw, static_cast<int>(ParamEditorEditMode::Draw));
    m_editModeGroup->addButton(m_btnErase, static_cast<int>(ParamEditorEditMode::Erase));
    m_editModeGroup->addButton(m_btnBake, static_cast<int>(ParamEditorEditMode::Bake));
    m_editModeGroup->addButton(m_btnShape, static_cast<int>(ParamEditorEditMode::Shape));
    m_editModeGroup->addButton(m_btnScale, static_cast<int>(ParamEditorEditMode::Scale));
    m_editModeGroup->addButton(m_btnAnchor, static_cast<int>(ParamEditorEditMode::Anchor));
    m_btnDraw->setChecked(true);

    auto *layout = new QHBoxLayout;
    layout->addWidget(m_btnDraw);
    layout->addWidget(m_btnErase);
    layout->addWidget(m_btnBake);
    layout->addWidget(m_btnShape);
    layout->addWidget(m_btnScale);
    layout->addWidget(m_btnAnchor);
    layout->setSpacing(4);
    layout->setContentsMargins({});
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setLayout(layout);

    connect(m_editModeGroup, &QButtonGroup::idToggled, this,
            [this](const int id, const bool checked) {
                if (checked)
                    emit editModeChanged(static_cast<ParamEditorEditMode>(id));
            });

    retranslateUi();
}

void ParamEditToolBarView::setBakeEnabled(const bool enabled) {
    m_btnBake->setEnabled(enabled);
    if (!enabled && m_btnBake->isChecked())
        m_btnDraw->setChecked(true);
}

void ParamEditToolBarView::setTransformEnabled(const bool enabled) {
    m_btnShape->setEnabled(enabled);
    m_btnScale->setEnabled(enabled);
    if (!enabled && (m_btnShape->isChecked() || m_btnScale->isChecked()))
        m_btnDraw->setChecked(true);
}

void ParamEditToolBarView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void ParamEditToolBarView::retranslateUi() {
    m_btnDraw->setToolTip(tr("Draw"));
    m_btnErase->setToolTip(tr("Erase"));
    m_btnShape->setToolTip(tr("Shape"));
    m_btnScale->setToolTip(tr("Scale"));
    m_btnBake->setToolTip(tr("Bake"));
    m_btnAnchor->setToolTip(tr("Anchor"));
}
