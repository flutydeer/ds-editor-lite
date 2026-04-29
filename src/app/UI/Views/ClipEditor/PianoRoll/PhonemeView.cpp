//
// Created by fluty on 24-2-12.
//

#include "PhonemeView.h"

#include "NoteView.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Utils/TrackColorPalette.h"
#include "UI/Utils/WaveformPainter.h"
#include "UI/Controls/ToolTip.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QThreadPool>
#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>

PhonemeView::PhonemeView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("PhonemeView");
    installEventFilter(this);

    connect(appModel, &AppModel::tempoChanged, this, &PhonemeView::onTempoChanged);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &PhonemeView::setPosition);
}

void PhonemeView::setDataContext(SingingClip *clip) {
    clip == nullptr ? moveToNullClipState() : moveToSingingClipState(clip);
}

void PhonemeView::updateNoteTime(Note *note) {
    m_notes.removeOne(note);
    MathUtils::binaryInsert(m_notes, note);
    resetPhonemeList();
    buildPhonemeList();
    update();
}

void PhonemeView::reset() {
    m_notes.clear();
    resetPhonemeList();
    update();
}

void PhonemeView::setTimeRange(const double startTick, const double endTick) {
    m_startTick = startTick;
    m_endTick = endTick;
    const auto ticksPerPixel = (m_endTick - m_startTick) / rect().width();
    m_resizeToleranceInTick = ticksPerPixel * AppGlobal::resizeTolerance;
    update();
}

void PhonemeView::setPosition(const double tick) {
    m_position = tick;
    update();
}

void PhonemeView::onTempoChanged() {
    if (m_clip) {
        resetPhonemeList();
        buildPhonemeList();
        update();
    }
}

void PhonemeView::onClipPropertyChanged() {
    // qDebug() << "PhonemeView::onClipPropertyChanged";
    moveToSingingClipState(m_clip);
    update();
}

void PhonemeView::onNoteChanged(const SingingClip::NoteChangeType type,
                                const QList<Note *> &notes) {
    switch (type) {
        case SingingClip::Insert:
            for (const auto &note : notes)
                MathUtils::binaryInsert(m_notes, note);
            break;
        case SingingClip::Remove:
            for (const auto &note : notes)
                m_notes.removeOne(note);
            break;
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &note : notes) {
                m_notes.removeOne(note);
                MathUtils::binaryInsert(m_notes, note);
            }
            break;
        case SingingClip::OriginalWordPropertyChange:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::EditedPhonemeOffsetChange:
            break;
    }

    resetPhonemeList();
    buildPhonemeList();
    update();
}

void PhonemeView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        emit wheelHorScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        emit wheelHorScroll(event);
    }
}

void PhonemeView::paintEvent(QPaintEvent *event) {
    QElapsedTimer timer;
    timer.start();

    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;

    if (!canEdit()) {
        painter.setPen(QColor(255, 255, 255, 80));
        painter.drawText(rect(), tr("Zoom in to edit phonemes"), QTextOption(Qt::AlignCenter));
        return;
    }

    auto originalColor = QColor(180, 180, 180);
    auto editedColor = TrackColorPalette::instance()->phonemeEdited(NoteView::trackColorIndex());
    auto fillColor = TrackColorPalette::instance()->phonemeFill(NoteView::trackColorIndex());
    auto positionLineColor = QColor(200, 200, 200);
    auto noteBoundaryColor = QColor(49, 53, 63);

    drawWaveforms(&painter);
    // painter.setPen(Qt::NoPen);
    // painter.setBrush(QColor(28, 29, 30));
    // painter.drawRect(rect());
    // painter.setBrush(Qt::NoBrush);

    auto drawSolidRect = [&](const double startTick, const double endTick, const QColor &color) {
        const auto start = tickToX(startTick);
        const auto length = tickToX(endTick) - start;
        const auto rectF = QRectF(start, 0, length, rect().height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRect(rectF);
    };

    auto drawSolidLine = [&](const double tick, const double penWidth, const QColor &color) {
        const auto x = tickToX(tick);
        pen.setColor(color);
        pen.setWidthF(penWidth);
        painter.setPen(pen);
        painter.drawLine(QLineF(x, 0, x, rect().height()));
    };

    auto drawPhoneName = [&](const PhonemeViewModel *phoneme) {
        const auto start = tickToX(phoneme->start + phoneme->startOffset);
        double endTick;
        if (phoneme->next) {
            endTick = phoneme->next->start + phoneme->next->startOffset;
        } else {
            endTick = phoneme->noteStart + phoneme->noteLength;
        }
        const auto end = tickToX(endTick);
        const auto length = end - start;
        const bool edited = phoneme->nameEdited;
        const auto textRect = QRectF(start + 2, 0, length - 4, rect().height());
        const auto penColor = edited ? editedColor : originalColor;
        painter.setPen(penColor);
        painter.setBrush(fillColor);

        QString text;
        if (phoneme->language == phoneme->noteLanguage)
            text = phoneme->name;
        else
            text = QString("%1/%2").arg(phoneme->language).arg(phoneme->name);

        auto font = painter.font();
        auto color = penColor;
        auto devicePixelRatio = painter.device()->devicePixelRatio();
        auto key = TextPixmapCache::Key{
            .text = text, .font = font, .color = color, .devicePixelRatio = devicePixelRatio};
        auto cache = TextPixmapCache::instance();
        if (!cache->contains(key)) {
            const QSize textSize = painter.fontMetrics().size(Qt::TextSingleLine, text);
            QPixmap pixmap(textSize * devicePixelRatio);
            pixmap.setDevicePixelRatio(devicePixelRatio);
            pixmap.fill(Qt::transparent);
            QPainter cachePainter(&pixmap);
            cachePainter.setPen(color);
            cachePainter.drawText(pixmap.rect(), text);
            cache->insert(key, pixmap);
        }

        const auto pixmapWidth = cache->get(key).width() / devicePixelRatio;
        const auto pixmapHeight = cache->get(key).height() / devicePixelRatio;
        const auto availableWidth = length - 4;
        const auto availableHeight = rect().height();
        double pixmapX;
        if (pixmapWidth <= availableWidth) {
            pixmapX = start + 2 + (availableWidth - pixmapWidth) / 2.0;
        } else {
            pixmapX = start + 2;
        }
        const auto pixmapY = (availableHeight - pixmapHeight) / 2.0;
        painter.drawPixmap(QPointF(pixmapX, pixmapY), cache->get(key));
    };

    if (canEdit()) {
        // Draw notes' word boundary
        for (int i = 0; i < m_notes.count(); ++i) {
            auto curNote = m_notes.at(i);
            if (curNote->globalStart() < m_startTick)
                continue;
            if (curNote->globalStart() > m_endTick)
                break;

            if (!curNote->isSlur())
                drawSolidLine(curNote->globalStart(), 1, noteBoundaryColor);

            if (i < m_notes.count() - 1) {
                auto nextNote = m_notes.at(i + 1);
                if (!nextNote->isSlur())
                    drawSolidLine(curNote->globalStart() + curNote->length(), 1, noteBoundaryColor);
            } else
                drawSolidLine(curNote->globalStart() + curNote->length(), 1, noteBoundaryColor);
        }

        // TODO： use binary find
        for (const auto phoneme : m_phonemes) {
            if (phoneme->start < m_startTick)
                continue;
            if (phoneme->start > m_endTick)
                break;
            if (phoneme->type == PhonemeViewModel::Sil)
                continue;

            auto start = phoneme->start + phoneme->startOffset;
            auto phonemePenWidth = phoneme->hoverOnControlBar ? 2.5 : 1.5;
            painter.setRenderHint(QPainter::Antialiasing);
            if (phoneme->offsetReady) {
                drawSolidLine(start, phonemePenWidth,
                              phoneme->offsetEdited ? editedColor : originalColor);
                // drawSolidLine(start, phonemePenWidth, originalColor);
                drawPhoneName(phoneme);
            }
        }
    }

    // Draw playback indicator
    painter.setRenderHint(QPainter::Antialiasing);
    auto penWidth = 1.0;
    pen.setWidthF(penWidth);
    pen.setColor(positionLineColor);
    painter.setPen(pen);
    auto x = tickToX(m_position);
    painter.drawLine(QLineF(x, 0, x, rect().height()));

    // const auto time = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    // qDebug() << "PhonemeView painted in" << time << "ms";
}

void PhonemeView::mousePressEvent(QMouseEvent *event) {
    if (!canEdit()) {
        event->ignore();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;


    appStatus->currentEditObject = AppStatus::EditObjectType::Phoneme;
    m_mouseMoved = false;
    m_mouseDownX = event->pos().x();
    const auto tick = xToTick(event->pos().x());
    if (const auto phoneme = phonemeAtTick(tick)) {
        m_freezeHoverEffects = true;
        m_curPhoneme = phoneme;
        m_mouseMoveBehavior = Move;

        m_currentLengthInMs = calculatePhonemeLengthInMs(*phoneme);

        if (!m_tooltip) {
            m_tooltip = new ToolTip(QString("%1 ms").arg(m_currentLengthInMs), this);
        }

        m_tooltip->setWindowOpacity(1);
        const auto cursorPos = QCursor::pos();
        m_tooltip->move(cursorPos.x(), cursorPos.y());
        m_tooltip->show();
    } else {
        QWidget::mousePressEvent(event);
        event->ignore();
    }
}

void PhonemeView::mouseMoveEvent(QMouseEvent *event) {
    if (!canEdit())
        return;
    // if (event->button() != Qt::LeftButton)
    //     return;

    m_mouseMoved = true;
    auto deltaTick = qRound(xToTick(event->pos().x()) - xToTick(m_mouseDownX));
    if (m_mouseMoveBehavior == Move) {
        const auto cur = m_curPhoneme;
        const auto prior = cur->prior;
        const auto curTargetStart = cur->start + deltaTick;
        if (curTargetStart <= prior->start)
            deltaTick = prior->start - cur->start;
        if (const auto next = cur->next) {
            const auto nextStart = next->start;
            if (curTargetStart > nextStart)
                deltaTick = nextStart - cur->start;
        }

        cur->startOffset = deltaTick;

        m_currentLengthInMs = calculatePhonemeLengthInMs(*cur);

        if (m_tooltip) {
            m_tooltip->setTitle(QString("%1 ms").arg(m_currentLengthInMs));
            const auto cursorPos = QCursor::pos();
            m_tooltip->move(cursorPos.x(), cursorPos.y());
        }
    }

    update();
}

void PhonemeView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouseMoveBehavior == Move && m_mouseMoved)
        handleAdjustCompleted(m_curPhoneme);

    if (m_tooltip) {
        m_tooltip->setWindowOpacity(0);
        m_tooltip->hide();
        m_tooltip->deleteLater();
        m_tooltip = nullptr;
    }
    m_mouseMoved = false;
    m_mouseMoveBehavior = None;
    m_freezeHoverEffects = false;
    updateHoverEffects();
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    QWidget::mouseReleaseEvent(event);
}

void PhonemeView::updateHoverEffects() {
    const auto pos = mapFromGlobal(QCursor::pos());
    const auto tick = xToTick(pos.x());
    if (const auto phoneme = phonemeAtTick(tick)) {
        setCursor(Qt::SizeHorCursor);
        phoneme->hoverOnControlBar = true;
        clearHoverEffects(phoneme);
        update();
    } else {
        setCursor(Qt::ArrowCursor);
        clearHoverEffects();
        update();
    }
}

bool PhonemeView::eventFilter(QObject *object, QEvent *event) {
    if (m_freezeHoverEffects)
        return QWidget::eventFilter(object, event);

    if (canEdit()) {
        if (event->type() == QEvent::HoverEnter) {
        } else if (event->type() == QEvent::HoverMove) {
            updateHoverEffects();
        } else if (event->type() == QEvent::HoverLeave) {
            setCursor(Qt::ArrowCursor);
            clearHoverEffects();
            update();
        }
    }

    return QWidget::eventFilter(object, event);
}

void PhonemeView::moveToSingingClipState(SingingClip *clip) {
    m_notes.clear();
    resetPhonemeList();
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }
    clearPieceWaveforms();

    m_clip = clip;
    setEnabled(true);

    if (clip->notes().count() > 0)
        for (const auto note : clip->notes())
            MathUtils::binaryInsert(m_notes, note);

    connect(clip, &SingingClip::propertyChanged, this, &PhonemeView::onClipPropertyChanged);
    connect(clip, &SingingClip::noteChanged, this, &PhonemeView::onNoteChanged);
    connect(clip, &SingingClip::piecesChanged, this, &PhonemeView::onPiecesChanged);

    for (const auto piece : clip->pieces()) {
        connect(piece, &InferPiece::statusChanged, this,
                [this, piece](InferStatus s) { onPieceStatusChanged(piece, s); });
        if (piece->acousticInferStatus == Success)
            loadWaveformAsync(piece);
    }

    resetPhonemeList();
    buildPhonemeList();
    update();
}

void PhonemeView::moveToNullClipState() {
    m_notes.clear();

    if (m_clip)
        disconnect(m_clip, nullptr, this, nullptr);
    m_clip = nullptr;

    clearPieceWaveforms();
    resetPhonemeList();
    update();
}

double PhonemeView::tickToX(const double tick) const {
    const auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    const auto x = rect().width() * ratio;
    return x;
}

double PhonemeView::xToTick(const double x) const {
    auto tick = 1.0 * x / rect().width() * (m_endTick - m_startTick) + m_startTick;
    if (tick < 0)
        tick = 0;
    return tick;
}

double PhonemeView::ticksPerPixel() const {
    return (m_endTick - m_startTick) / rect().width();
}

bool PhonemeView::canEdit() const {
    return ticksPerPixel() < m_canEditTicksPerPixelThreshold;
}

PhonemeView::PhonemeViewModel *PhonemeView::phonemeAtTick(const double tick) {
    for (const auto phoneme : m_phonemes) {
        if (phoneme->type == PhonemeViewModel::Sil)
            continue;
        if (qAbs(tick - phoneme->start) < m_resizeToleranceInTick)
            return phoneme;
    }
    return nullptr;
}

QList<PhonemeView::PhonemeViewModel *> PhonemeView::findPhonemesByNoteId(int noteId) {
    return Linq::where(m_phonemes, [=](auto p) { return p->noteId == noteId; });
}

// TODO: 只处理有效片段
void PhonemeView::buildPhonemeList() {
    // qDebug() << "build phoneme list";
    if (m_notes.count() == 0)
        return;

    const auto head = new PhonemeViewModel;
    head->noteId = -1;
    head->type = PhonemeViewModel::Sil;
    head->start = -INT_MAX;
    head->prior = nullptr;
    head->next = nullptr;
    PhonemeViewModel *prior = head;
    m_phonemes.append(head);

    double lastNoteEndTick = -INT_MAX;

    auto insertNextNode = [](PhonemeViewModel *p1, PhonemeViewModel *p2) {
        p2->next = p1->next;
        if (p1->next != nullptr)
            p1->next->prior = p2;
        p2->prior = p1;
        p1->next = p2;
    };

    auto insertSilPhoneme = [&](double silStart) {
        const auto sil = new PhonemeViewModel;
        sil->type = PhonemeViewModel::Sil;
        sil->noteId = -1;
        sil->start = silStart;
        sil->offsetReady = true;
        m_phonemes.append(sil);
        insertNextNode(prior, sil);
        prior = sil;
    };

    for (int i = 0; i < m_notes.count(); ++i) {
        const auto note = m_notes.at(i);
        if (note->isSlur())
            continue;
        if (note->overlapped())
            continue;

        const auto noteStartTick = note->globalStart();
        auto noteEndTick = noteStartTick + note->length();

        // Find consecutive slur notes and extend the end tick
        for (int j = i + 1; j < m_notes.count(); ++j) {
            const auto nextNote = m_notes.at(j);
            if (!nextNote->isSlur())
                break;
            if (nextNote->overlapped())
                break;
            // Check if the slur note is continuous (no gap)
            const auto nextStart = nextNote->globalStart();
            if (nextStart > noteEndTick)
                break;
            noteEndTick = nextStart + nextNote->length();
        }

        // Insert Sil phoneme if there is a gap between notes
        if (noteStartTick > lastNoteEndTick && lastNoteEndTick > -INT_MAX) {
            insertSilPhoneme(lastNoteEndTick);
        }

        const auto noteStartMs = appModel->tickToMs(noteStartTick);
        {
            auto names = note->phonemeNameSeq();
            auto offsets = note->phonemeOffsetSeq();
            for (int k = 0; k < names.result().count(); k++) {
                const auto model = new PhonemeViewModel;
                model->type = PhonemeViewModel::Normal;
                model->noteId = note->id();
                model->noteStart = note->globalStart();
                model->noteLength = note->length();
                model->noteLanguage = note->language();
                model->nameEdited = names.isEdited();
                model->offsetEdited = offsets.isEdited();
                model->language = names.result().at(k).language;
                model->name = names.result().at(k).name;
                model->isOnset = names.result().at(k).isOnset;
                if (!offsets.result().isEmpty()) {
                    model->offsetReady = true;
                    const auto phoneStartMs = noteStartMs + offsets.result().at(k);
                    const auto phoneStartTick = qRound(appModel->msToTick(phoneStartMs));
                    model->start = phoneStartTick;
                }
                m_phonemes.append(model);
                insertNextNode(prior, model);
                prior = model;
            }
        }

        lastNoteEndTick = noteEndTick;
    }
}

void PhonemeView::resetPhonemeList() {
    for (const auto phoneme : m_phonemes)
        delete phoneme;
    m_phonemes.clear();
}

void PhonemeView::clearHoverEffects(const PhonemeViewModel *except) {
    for (const auto item : m_phonemes) {
        if (item != except && item->hoverOnControlBar)
            item->hoverOnControlBar = false;
    }
}

void PhonemeView::handleAdjustCompleted(const PhonemeViewModel *phVm) {
    QList<int> offsets;
    const auto phonemes = findPhonemesByNoteId(phVm->noteId);
    auto relatedPhonemes =
        Linq::where(phonemes, [&](const PhonemeViewModel *p) { return p->type == phVm->type; });
    if (relatedPhonemes.isEmpty()) {
        qFatal() << "handleAdjustCompleted: related phonemes is empty";
        return;
    }
    const auto note = m_clip->findNoteById(phVm->noteId);
    Q_ASSERT(note);
    const auto noteStartInMs = appModel->tickToMs(note->globalStart());
    if (phVm->type == PhonemeViewModel::Normal) {
        for (const auto phoneme : relatedPhonemes) {
            const auto phonemeStartInMs = appModel->tickToMs(phoneme->start + phoneme->startOffset);
            offsets.append(qRound(phonemeStartInMs - noteStartInMs));
        }
    } else {
        qFatal() << "handleAdjustCompleted: adjusted Sil phoneme";
        return;
    }

    if (m_curPhoneme) {
        m_curPhoneme->startOffset = 0;
        m_curPhoneme = nullptr;
    }
    clipController->onAdjustPhonemeOffset(phVm->noteId, offsets);
}

int PhonemeView::calculatePhonemeLengthInMs(const PhonemeViewModel &phoneme) const {
    const auto phonemeStartTick = phoneme.start + phoneme.startOffset;
    double phonemeEndTick;
    if (phoneme.next) {
        phonemeEndTick = phoneme.next->start + phoneme.next->startOffset;
    } else {
        phonemeEndTick = phoneme.noteStart + phoneme.noteLength;
    }
    const auto phonemeStartMs = appModel->tickToMs(phonemeStartTick);
    const auto phonemeEndMs = appModel->tickToMs(phonemeEndTick);
    return qRound(phonemeEndMs - phonemeStartMs);
}

void PhonemeView::onPiecesChanged(const PieceList &pieces, const PieceList &newPieces,
                                  const PieceList &discardedPieces) {
    for (const auto piece : discardedPieces) {
        disconnect(piece, nullptr, this, nullptr);
        if (m_pieceWaveforms.contains(piece)) {
            delete m_pieceWaveforms[piece].painter;
            m_pieceWaveforms.remove(piece);
        }
    }
    for (const auto piece : newPieces) {
        connect(piece, &InferPiece::statusChanged, this,
                [this, piece](InferStatus s) { onPieceStatusChanged(piece, s); });
        if (piece->acousticInferStatus == Success)
            loadWaveformAsync(piece);
    }
    update();
}

void PhonemeView::onPieceStatusChanged(InferPiece *piece, InferStatus status) {
    if (status == Success) {
        loadWaveformAsync(piece);
    } else {
        if (m_pieceWaveforms.contains(piece)) {
            delete m_pieceWaveforms[piece].painter;
            m_pieceWaveforms.remove(piece);
            update();
        }
    }
}

void PhonemeView::onWaveformReady(InferPiece *piece, const AudioInfoModel &info) {
    if (!m_clip)
        return;
    if (!m_clip->pieces().contains(piece))
        return;

    const int clipOffset = m_clip->start();
    auto *painter = new WaveformPainter;
    painter->setAudioPath(piece->audioPath);
    painter->setAudioInfo(info);
    painter->setTempo(appModel->tempo());

    PieceWaveform wf;
    wf.painter = painter;
    wf.globalStartTick = piece->localStartTick() + clipOffset;
    wf.globalEndTick = piece->localEndTick() + clipOffset;

    if (m_pieceWaveforms.contains(piece))
        delete m_pieceWaveforms[piece].painter;
    m_pieceWaveforms[piece] = wf;
    update();
}

void PhonemeView::loadWaveformAsync(InferPiece *piece) {
    const QString audioPath = piece->audioPath;
    if (audioPath.isEmpty())
        return;

    QThreadPool::globalInstance()->start([this, piece, audioPath]() {
        auto *fm = AudioContext::instance()->formatManager();
        if (!fm)
            return;
        auto *io = fm->getFormatLoad(audioPath);
        if (!io)
            return;
        if (!io->open(talcs::AbstractAudioFormatIO::Read)) {
            delete io;
            return;
        }

        AudioInfoModel info;
        info.sampleRate = io->sampleRate();
        info.channels = io->channelCount();
        info.frames = io->length();
        info.chunkSize = 128;
        info.mipmapScale = 10;

        std::vector<float> buffer(info.chunkSize * info.channels);
        long long samplesRead = 0;
        short min = 0, max = 0;
        int chunkIndex = 0;
        bool hasTail = false;

        while (samplesRead < info.frames) {
            const qint64 read = io->read(buffer.data(), info.chunkSize);
            if (read <= 0)
                break;
            samplesRead += read;

            double sampleMax = 0, sampleMin = 0;
            for (qint64 i = 0; i < read; i++) {
                double mono = 0.0;
                for (int ch = 0; ch < info.channels; ch++)
                    mono += buffer[i * info.channels + ch] / static_cast<double>(info.channels);
                if (mono > sampleMax)
                    sampleMax = mono;
                if (mono < sampleMin)
                    sampleMin = mono;
            }

            auto toShort = [](double d) -> short {
                if (d < -1) d = -1;
                else if (d > 1) d = 1;
                return static_cast<short>(d * 32767);
            };

            info.peakCache.append(std::make_tuple(toShort(sampleMin), toShort(sampleMax)));

            if ((chunkIndex + 1) % info.mipmapScale == 0) {
                info.peakCacheMipmap.append(std::make_tuple(min, max));
                min = 0;
                max = 0;
                hasTail = false;
            } else {
                if (toShort(sampleMin) < min) min = toShort(sampleMin);
                if (toShort(sampleMax) > max) max = toShort(sampleMax);
                hasTail = true;
            }
            chunkIndex++;
        }
        if (hasTail)
            info.peakCacheMipmap.append(std::make_tuple(min, max));

        io->close();
        delete io;

        QMetaObject::invokeMethod(this, [this, piece, info]() {
            onWaveformReady(piece, info);
        });
    });
}

void PhonemeView::drawWaveforms(QPainter *painter) {
    if (m_pieceWaveforms.isEmpty())
        return;

    const auto waveformColor = QColor(49, 53, 63);

    for (auto it = m_pieceWaveforms.constBegin(); it != m_pieceWaveforms.constEnd(); ++it) {
        const auto &wf = it.value();
        if (!wf.painter)
            continue;

        if (wf.globalEndTick <= m_startTick || wf.globalStartTick >= m_endTick)
            continue;

        const double x1 = tickToX(wf.globalStartTick);
        const double x2 = tickToX(wf.globalEndTick);
        const QRectF wfRect(x1, 0, x2 - x1, rect().height());

        wf.painter->paint(painter, wfRect, waveformColor,
                          static_cast<double>(wf.globalStartTick),
                          static_cast<double>(wf.globalEndTick));
    }
}

void PhonemeView::clearPieceWaveforms() {
    for (auto it = m_pieceWaveforms.begin(); it != m_pieceWaveforms.end(); ++it)
        delete it.value().painter;
    m_pieceWaveforms.clear();
}