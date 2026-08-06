#include "SingingClipView.h"

#include <QFile>
#include <QPainter>

#include "Global/TracksEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/Support/MathUtils.h>
#include "Global/AppGlobal.h"

#include <algorithm>

using namespace TracksEditorGlobal;

int SingingClipView::NoteViewModel::compareTo(const NoteViewModel *obj) const {
    const auto otherStart = obj->rStart;
    if (rStart < otherStart)
        return -1;
    if (rStart > otherStart)
        return 1;
    return 0;
}

bool SingingClipView::NoteViewModel::isOverlappedWith(NoteViewModel *obj) {
    return false;
}

std::tuple<qsizetype, qsizetype> SingingClipView::NoteViewModel::interval() const {
    return std::make_tuple(0, 0);
}

SingingClipView::SingingClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
    setCanResizeLength(true);
    connect(appStatus, &AppStatus::pianoRollVisibleRectChanged, this,
            [this](const QRectF &) { update(); });
}

SingingClipView::~SingingClipView() {
    disconnect();
    dispose();
}

void SingingClipView::loadNotes(const OverlappableSerialList<Note> &notes) {
    dispose();
    if (notes.count() != 0)
        for (const auto &note : notes)
            addNote(note);

    update();
}

void SingingClipView::loadPreviewNotes(const QVector<std::tuple<int, int, int>> &notes) {
    dispose();
    for (const auto &[start, length, key] : notes) {
        auto *viewModel = new NoteViewModel;
        viewModel->rStart = start;
        viewModel->length = length;
        viewModel->keyIndex = key;
        m_notes.append(viewModel);
    }
    std::sort(m_notes.begin(), m_notes.end(),
              [](const NoteViewModel *left, const NoteViewModel *right) {
                  return left->rStart < right->rStart;
              });
    update();
}

int SingingClipView::contentLength() const {
    if (m_notes.isEmpty())
        return AppGlobal::ticksPerWholeNote;
    const auto lastNote = m_notes.last();
    return lastNote->rStart + lastNote->length;
}

void SingingClipView::onNoteListChanged(const SingingClip::NoteChangeType type,
                                        const QList<Note *> &notes) {
    switch (type) {
        case SingingClip::Insert:
            for (const auto &note : notes)
                addNote(note);
            break;
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &note : notes) {
                removeNote(note->id());
                addNote(note);
            }
            break;
        case SingingClip::Remove:
            for (const auto &note : notes)
                removeNote(note->id());
            break;
        default:
            break;
    }
    update();
}

void SingingClipView::onNotePropertyChanged(const Note *note) {
    removeNote(note->id());
    addNote(note);
}

void SingingClipView::setSingerName(const QString &singerName) {
    m_singerName = singerName;
    update();
}

void SingingClipView::setSpeakerName(const QString &speakerName) {
    m_speakerName = speakerName;
    update();
}

void SingingClipView::setDefaultLanguage(const QString &language) {
    m_language = language;
    update();
}

QString SingingClipView::text() const {
    return AbstractClipView::text() + (!m_singerName.isEmpty() ? m_singerName : tr("(No singer)")) +
           (!m_speakerName.isEmpty() ? (" / " + m_speakerName) : "") + " " + m_language + " ";
}

void SingingClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                      const QColor color) {
    painter->setRenderHint(QPainter::Antialiasing);

    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();

    if (rectHeight < 32 || rectWidth < 16)
        return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    const auto layout = computeNoteLayout(previewRect);
    const auto noteHeight = layout.noteHeight;
    const auto highestKeyIndex = layout.highestKeyIndex;

    for (const auto &note : m_notes) {
        const auto clipLeft = start() + clipStart();
        const auto clipRight = clipLeft + clipLen();
        if (start() + note->rStart + note->length < clipLeft)
            continue;
        if (start() + note->rStart >= clipRight)
            break;

        const auto leftScene = tickToSceneX(start() + note->rStart);
        auto left = sceneXToItemX(leftScene);
        auto width = tickToSceneX(note->length);
        if (start() + note->rStart < clipLeft) {
            left = sceneXToItemX(tickToSceneX(clipLeft));
            width = sceneXToItemX(tickToSceneX(start() + note->rStart + note->length)) - left;
        } else if (start() + note->rStart + note->length >= clipRight)
            width = tickToSceneX(clipRight - start() - note->rStart);
        const auto top = -(note->keyIndex - highestKeyIndex) * noteHeight + rectTop;
        painter->drawRect(QRectF(left, top, width, noteHeight));
    }

    drawPianoRollOverlay(painter, previewRect, noteHeight, highestKeyIndex);
}

void SingingClipView::drawPianoRollOverlay(QPainter *painter, const QRectF &previewRect,
                                           const double noteHeight, const int highestKeyIndex) {
    if (!activeClip() || m_notes.isEmpty() || noteHeight <= 0)
        return;

    // x 轴为相对 active clip 的局部 tick，用 clip 当前 start() 平移成全局 tick，
    // 拖动 clip 时（view start 实时变化）叠加层跟随 clip 移动
    const QRectF viewportRect = appStatus->pianoRollVisibleRect;
    if (viewportRect.isNull() || viewportRect.isEmpty())
        return;
    const double prStartTick = viewportRect.left() + static_cast<double>(start());
    const double prEndTick = viewportRect.right() + static_cast<double>(start());
    const double prLowKey = viewportRect.top();     // 低音（QRectF y 轴向下 = keyIndex 减小）
    const double prHighKey = viewportRect.bottom(); // 高音

    // x 轴：与 clip 可见范围求交集
    const double clipLeft = static_cast<double>(start() + clipStart());
    const double clipRight = static_cast<double>(clipLeft + clipLen());
    const double overlayTickStart = std::max(prStartTick, clipLeft);
    const double overlayTickEnd = std::min(prEndTick, clipRight);
    if (overlayTickEnd <= overlayTickStart)
        return;

    const auto left = sceneXToItemX(tickToSceneX(overlayTickStart));
    const auto right = sceneXToItemX(tickToSceneX(overlayTickEnd));

    // y 轴：复用 drawPreviewArea 的 noteHeight + highestKeyIndex 映射
    const auto rectTop = previewRect.top();
    const auto overlayTop = -(prHighKey - highestKeyIndex) * noteHeight + rectTop;
    const auto overlayBottom = -(prLowKey - highestKeyIndex) * noteHeight + rectTop + noteHeight;
    const auto overlayRect = QRectF(left, overlayTop, right - left, overlayBottom - overlayTop);

    // 边框色跟随轨道色（AppColorPalette 由主题系统驱动）
    const auto borderColor = AppColorPalette::instance()->clipBorder(colorIndex());
    QPen pen(borderColor, 1.2);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(overlayRect, AbstractClipView::clipCornerRadius,
                             AbstractClipView::clipCornerRadius);
}

SingingClipView::NoteLayout SingingClipView::computeNoteLayout(const QRectF &previewRect) const {
    NoteLayout layout;
    for (const auto note : m_notes) {
        const auto keyIndex = note->keyIndex;
        if (keyIndex < layout.lowestKeyIndex)
            layout.lowestKeyIndex = keyIndex;
        if (keyIndex > layout.highestKeyIndex)
            layout.highestKeyIndex = keyIndex;
    }
    const int divideCount = layout.highestKeyIndex - layout.lowestKeyIndex + 1;
    layout.noteHeight = previewRect.height() / divideCount;
    if (layout.noteHeight > 16)
        layout.noteHeight = 16;
    return layout;
}

double SingingClipView::keyIndexAtScenePos(const QPointF &scenePos) const {
    const auto preview = previewRect();
    if (preview.height() < 32 || m_notes.isEmpty())
        return -1.0;
    const auto layout = computeNoteLayout(preview);
    if (!layout.valid())
        return -1.0;
    const double localY = mapFromScene(scenePos).y();
    if (localY < preview.top() || localY > preview.bottom())
        return -1.0;
    // 逆映射：y = -(keyIndex - highestKeyIndex) * noteHeight + preview.top()
    return layout.highestKeyIndex - (localY - preview.top()) / layout.noteHeight;
}

QString SingingClipView::clipTypeName() const {
    return tr("[Singing] ");
}

QString SingingClipView::iconPath() const {
    return ":svg/icons/midi_clip_16_filled.svg";
}

void SingingClipView::addNote(const Note *note) {
    const auto vm = new NoteViewModel;
    vm->id = note->id();
    vm->rStart = note->localStart();
    vm->length = note->length();
    vm->keyIndex = note->keyIndex();
    MathUtils::binaryInsert(m_notes, vm);
}

void SingingClipView::removeNote(const int id) {
    for (const auto note : m_notes) {
        if (note->id == id) {
            m_notes.removeOne(note);
            delete note;
            return;
        }
    }
    qWarning() << "removeNote: item not found:" << id;
}

void SingingClipView::dispose() {
    for (const auto note : m_notes)
        delete note;
    m_notes.clear();
}
