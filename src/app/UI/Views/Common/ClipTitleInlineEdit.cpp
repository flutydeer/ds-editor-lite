#include "ClipTitleInlineEdit.h"

#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>

#include "Controller/TrackController.h"

#include <QFontMetricsF>
#include <QLineEdit>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {
    // The ClipTitle role pads the line edit by 4px on each side - the same
    // inset the title text is painted at - and the caret needs a little room on
    // top of the widest text it may hold.
    constexpr int kTextPadding = 12;
    // The overlay refuses to get smaller than its line edit's minimum height.
    constexpr int kEditorMinimumHeight = 20;
}

ClipTitleInlineEdit::ClipTitleInlineEdit(QWidget *host, QObject *parent)
    : QObject(parent), m_host(host) {
}

void ClipTitleInlineEdit::begin(const int clipId, const QRectF &titleRect, const QString &name,
                                const QFont &font) {
    if (!m_host || clipId < 0 || titleRect.isEmpty())
        return;
    if (m_clipId == clipId && isEditing())
        return;
    // The target is moving on: commit what the previous clip was showing before
    // the overlay is repointed at the new one.
    submit();

    ensureOverlay();
    if (!m_overlay)
        return;

    m_clipId = clipId;
    m_font = font;
    m_anchorBand = titleRect.toAlignedRect();
    // The ClipTitle role is themed for the canvas, with the text inset the
    // painted title uses so the name does not shift when the editor opens.
    const QVariantMap editorProperties = {
        {QStringLiteral("editRole"), QStringLiteral("ClipTitle")}
    };
    m_overlay->showAt(editorRectFor(name), name, font, editorProperties);

    // Keep the field hugging the text while it is being typed. A line edit that
    // is wider than its content reads as a text box, not as the clip's name.
    if (m_textChangedConnection)
        QObject::disconnect(m_textChangedConnection);
    if (auto *lineEdit = m_overlay->findChild<QLineEdit *>()) {
        m_textChangedConnection = connect(lineEdit, &QLineEdit::textChanged, this,
                                          [this](const QString &text) { applyEditorWidth(text); });
    }
}

int ClipTitleInlineEdit::editorWidthFor(const QString &text, const QFont &font,
                                        const int bandWidth) {
    const QFontMetricsF metrics(font);
    const auto textWidth = static_cast<int>(std::ceil(metrics.horizontalAdvance(text))) + kTextPadding;
    // Never wider than the band the name belongs to, unless that band is too
    // narrow to type in at all - then the minimum wins and the field spills.
    const auto upperBound = std::max(
        minimumEditorWidth, std::min(maximumEditorWidth, std::max(0, bandWidth)));
    return std::clamp(std::max(textWidth, 0), minimumEditorWidth, upperBound);
}

int ClipTitleInlineEdit::editorWidthFor(const QString &text) const {
    return editorWidthFor(text, m_font, m_anchorBand.width());
}

QRect ClipTitleInlineEdit::editorRectFor(const QString &text) const {
    auto rect = m_anchorBand;
    const auto hostRect = m_host->rect();
    rect.setWidth(editorWidthFor(text));
    rect.setHeight(std::clamp(rect.height(), kEditorMinimumHeight,
                              std::max(kEditorMinimumHeight, hostRect.height())));
    if (hostRect.width() > 0)
        rect.setWidth(std::min(rect.width(), hostRect.width()));
    // Growing to the right is the natural reading order, but a clip near the
    // edge keeps the field on screen by sliding it left instead.
    rect.moveLeft(std::clamp(rect.left(), 0, std::max(0, hostRect.width() - rect.width())));
    rect.moveTop(std::clamp(rect.top(), 0, std::max(0, hostRect.height() - rect.height())));
    return rect;
}

void ClipTitleInlineEdit::applyEditorWidth(const QString &text) {
    if (m_clipId < 0 || !m_overlay || !m_overlay->isEditing())
        return;
    m_overlay->setGeometry(editorRectFor(text));
}

void ClipTitleInlineEdit::submit() {
    if (m_overlay && m_overlay->isEditing())
        m_overlay->submit();
    else
        endSession();
}

void ClipTitleInlineEdit::cancel() {
    if (m_overlay && m_overlay->isEditing())
        m_overlay->dismiss(true);
    else
        endSession();
}

bool ClipTitleInlineEdit::isEditing() const {
    return m_overlay && m_overlay->isEditing();
}

int ClipTitleInlineEdit::editingClipId() const {
    return m_clipId;
}

bool ClipTitleInlineEdit::containsViewportPoint(const QPointF &viewportPosition) const {
    return isEditing() && m_overlay->geometry().contains(viewportPosition.toPoint());
}

bool ClipTitleInlineEdit::relayTouchBegin(const QPointF &viewportPosition) {
    if (!containsViewportPoint(viewportPosition))
        return false;
    return m_overlay->relayTouchBegin(m_host->mapToGlobal(viewportPosition.toPoint()));
}

void ClipTitleInlineEdit::relayTouchMove(const QPointF &viewportPosition) {
    if (m_overlay)
        m_overlay->relayTouchMove(m_host->mapToGlobal(viewportPosition.toPoint()));
}

void ClipTitleInlineEdit::relayTouchEnd(const QPointF &viewportPosition) {
    if (m_overlay)
        m_overlay->relayTouchEnd(m_host->mapToGlobal(viewportPosition.toPoint()));
}

void ClipTitleInlineEdit::relayTouchCancel() {
    if (m_overlay)
        m_overlay->relayTouchCancel();
}

void ClipTitleInlineEdit::ensureOverlay() {
    if (m_overlay)
        return;
    m_overlay = new InlineTextEditOverlay(m_host);
    connect(m_overlay.data(), &InlineTextEditOverlay::textSubmitted, this,
            &ClipTitleInlineEdit::onTextSubmitted);
    connect(m_overlay.data(), &InlineTextEditOverlay::editCancelled, this,
            &ClipTitleInlineEdit::endSession);
}

void ClipTitleInlineEdit::endSession() {
    const auto clipId = m_clipId;
    m_clipId = -1;
    if (clipId >= 0)
        emit sessionFinished(clipId);
}

void ClipTitleInlineEdit::onTextSubmitted(const QString &text) {
    const auto clipId = m_clipId;
    endSession();
    if (clipId < 0)
        return;

    const auto *clip = appModel->findClipById(clipId);
    if (!clip)
        return;

    const auto name = text.trimmed();
    if (name == clip->name())
        return;

    // The same call the clip editor toolbar uses, so the rename stays a single
    // undoable action whichever surface started it.
    auto args = Clip::ClipCommonProperties(*clip);
    args.name = name;
    trackController->onClipPropertyChanged(args);
}
