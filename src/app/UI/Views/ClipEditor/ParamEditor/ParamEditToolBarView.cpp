#include "ParamEditToolBarView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Utils/IconUtils.h>
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>

ParamEditToolBarView::ParamEditToolBarView(QWidget *parent) : QWidget(parent) {
    const auto createEditModeButton = [this](const char *objectName, const QString &iconPath) {
        auto *button = new Button;
        button->setObjectName(QString::fromLatin1(objectName));
        button->setCheckable(true);
        button->setFixedSize(24, 24);
        button->setIconSize({16, 16});
        m_tintedButtons.append({button, iconPath});
        return button;
    };

    m_btnDraw =
        createEditModeButton("btnParamDraw", QStringLiteral(":/svg/icons/edit_24_filled.svg"));
    m_btnErase =
        createEditModeButton("btnParamErase", QStringLiteral(":/svg/icons/eraser_24_filled.svg"));
    m_btnBake =
        createEditModeButton("btnParamBake", QStringLiteral(":/svg/icons/brush_24_filled.svg"));
    m_btnShape = createEditModeButton("btnParamShape",
                                      QStringLiteral(":/svg/icons/param_shape_24_filled.svg"));
    m_btnScale = createEditModeButton("btnParamScale",
                                      QStringLiteral(":/svg/icons/param_scale_24_filled.svg"));
    m_btnAnchor = createEditModeButton("btnParamAnchor",
                                       QStringLiteral(":/svg/icons/pitch_anchor_24_filled.svg"));
    rebuildIcons();

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

void ParamEditToolBarView::setParameter(const ParamInfo::Name parameter) {
    bool resetEditMode = false;
    for (auto *button : m_editModeGroup->buttons()) {
        const auto mode = static_cast<ParamEditorEditMode>(m_editModeGroup->id(button));
        const bool visible = isParamEditorEditModeVisible(mode, parameter);
        button->setVisible(visible);
        resetEditMode = resetEditMode || (button->isChecked() && !visible);
    }
    if (resetEditMode)
        m_btnDraw->setChecked(true);
}

ParamEditorEditMode ParamEditToolBarView::editMode() const {
    return static_cast<ParamEditorEditMode>(m_editModeGroup->checkedId());
}

bool ParamEditToolBarView::supportsEditMode(const ParamEditorEditMode mode) const {
    const auto *button = m_editModeGroup->button(static_cast<int>(mode));
    return button && !button->isHidden();
}

bool ParamEditToolBarView::setEditMode(const ParamEditorEditMode mode) {
    auto *button = m_editModeGroup->button(static_cast<int>(mode));
    if (!button || button->isHidden())
        return false;
    button->setChecked(true);
    return true;
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

void ParamEditToolBarView::rebuildIcons() {
    const QSize iconSize(16, 16);
    IconUtils::SvgIconToggleColorPalette palette;
    palette.off.normal = m_iconColor;
    palette.off.disabled = m_iconDisabledColor;
    palette.on.normal = m_iconOnColor;
    palette.on.disabled = m_iconOnDisabledColor;
    for (const auto &[btn, svgPath] : m_tintedButtons)
        btn->setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize, palette));
}

QColor ParamEditToolBarView::iconColor() const {
    return m_iconColor;
}

void ParamEditToolBarView::setIconColor(const QColor &color) {
    if (m_iconColor == color)
        return;
    m_iconColor = color;
    rebuildIcons();
}

QColor ParamEditToolBarView::iconDisabledColor() const {
    return m_iconDisabledColor;
}

void ParamEditToolBarView::setIconDisabledColor(const QColor &color) {
    if (m_iconDisabledColor == color)
        return;
    m_iconDisabledColor = color;
    rebuildIcons();
}

QColor ParamEditToolBarView::iconOnColor() const {
    return m_iconOnColor;
}

void ParamEditToolBarView::setIconOnColor(const QColor &color) {
    if (m_iconOnColor == color)
        return;
    m_iconOnColor = color;
    rebuildIcons();
}

QColor ParamEditToolBarView::iconOnDisabledColor() const {
    return m_iconOnDisabledColor;
}

void ParamEditToolBarView::setIconOnDisabledColor(const QColor &color) {
    if (m_iconOnDisabledColor == color)
        return;
    m_iconOnDisabledColor = color;
    rebuildIcons();
}
