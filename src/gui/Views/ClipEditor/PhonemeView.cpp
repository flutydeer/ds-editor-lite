//
// Created by fluty on 24-2-12.
//

#include <QMouseEvent>

#include "PhonemeView.h"
#include "Model/AppModel.h"
#include "Utils/AppGlobal.h"

using namespace AppGlobal;

PhonemeView::PhonemeView(QWidget *parent) {
    setAttribute(Qt::WA_Hover, true);
    installEventFilter(this);
}
void PhonemeView::insertNote(Note *note) {
    auto noteViewModel = new NoteViewModel;
    noteViewModel->id = note->id();
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
    noteViewModel->originalPhonemes = note->phonemes().original;
    noteViewModel->editedPhonemes = note->phonemes().edited;
    m_notes.add(noteViewModel);
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::removeNote(int noteId) {
    qDebug() << "PhonemeView::removeNote" << noteId;
    auto note = findNoteById(noteId);
    m_notes.remove(note);
    delete note;
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::updateNote(Note *note) {
    qDebug() << "PhonemeView::updateNote" << note->id() << note->lyric();
    auto noteViewModel = findNoteById(note->id());
    m_notes.remove(noteViewModel);
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
    noteViewModel->originalPhonemes = note->phonemes().original;
    noteViewModel->editedPhonemes = note->phonemes().edited;
    qDebug() << note->phonemes().edited.last().name << note->phonemes().edited.last().start;
    m_notes.add(noteViewModel);
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::reset() {
    for (auto note : m_notes)
        delete note;
    m_notes.clear();
    update();
}
void PhonemeView::setTimeRange(double startTick, double endTick) {
    m_startTick = startTick;
    m_endTick = endTick;
    auto ticksPerPixel = (m_endTick - m_startTick) / rect().width();
    m_resizeToleranceInTick = ticksPerPixel * AppGlobal::resizeTolarance;
    update();
}
void PhonemeView::setTimeSignature(int numerator, int denominator) {
    ITimelinePainter::setTimeSignature(numerator, denominator);
    update();
}
void PhonemeView::setPosition(double tick) {
    m_position = tick;
    update();
}
void PhonemeView::setQuantize(int quantize) {
    ITimelinePainter::setQuantize(quantize);
    update();
}
void PhonemeView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;
    auto penWidth = 2;
    pen.setWidthF(penWidth);
    auto appModel = AppModel::instance();

    auto mainColor = QColor(155, 186, 255);
    auto fillColor = QColor(155, 186, 255, 50);
    auto positionLineColor = QColor(255, 204, 153);

    // Draw background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(28, 29, 30));
    painter.drawRect(rect());
    painter.setBrush(Qt::NoBrush);

    // Draw graduates
    drawTimeline(&painter, m_startTick, m_endTick,
                 rect().width() - AppGlobal::verticalScrollBarWidth);

    auto drawSolidRect = [&](double startTick, double endTick) {
        auto start = tickToX(startTick);
        auto length = tickToX(endTick) - start;
        auto rectF = QRectF(start, 0, length, rect().height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRect(rectF);
    };

    auto drawSolidLine = [&](double tick, bool hoverOnControlBar) {
        auto x = tickToX(tick);
        pen.setColor(mainColor);
        pen.setWidthF(hoverOnControlBar ? 2.5 : 1.0);
        painter.setPen(pen);
        painter.drawLine(QLineF(x, 0, x, rect().height()));
    };

    auto drawPhoneName = [&](PhonemeViewModel *phoneme) {
        auto start = tickToX(phoneme->start + phoneme->startOffset);
        auto length = tickToX(phoneme->endWithOffset()) - start;
        auto textRect = QRectF(start + 2, 0, length - 4, rect().height());
        painter.setPen(mainColor);
        painter.setBrush(fillColor);

        auto fontMetrics = painter.fontMetrics();
        QString text;
        if (m_showDebugInfo)
            text = phoneme->name + QString(" s%1 so%2 l%3 lo%4")
                                       .arg(phoneme->start)
                                       .arg(phoneme->startOffset)
                                       .arg(phoneme->length)
                                       .arg(phoneme->lengthOffset);
        else
            text = phoneme->name;
        auto textWidth = fontMetrics.horizontalAdvance(text);
        if (textWidth > textRect.width())
            return;
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);
        painter.drawText(textRect, text, textOption);
    };

    for (const auto phoneme : m_phonemes) {
        if (phoneme->end() < m_startTick)
            continue;
        if (phoneme->start > m_endTick)
            break;
        if (phoneme->type == PhonemeViewModel::Sil)
            continue;

        auto start = phoneme->start + phoneme->startOffset;
        auto end = phoneme->endWithOffset();
        if (canEdit()) {
            drawSolidRect(start, end);
            drawSolidLine(start, phoneme->hoverOnControlBar);
            drawPhoneName(phoneme);
            /*} else if (ticksPerPixel < 10) {
                painter.setRenderHint(QPainter::Antialiasing, false);
                drawSolidRect(phoneme->start, phoneme->start + phoneme->length);
                drawSolidLine(phoneme->start);*/
        } else {
            painter.setRenderHint(QPainter::Antialiasing, false);
            drawSolidRect(start, end);
        }
        // qDebug() << "prior" << (phoneme->prior == nullptr ? "null" : phoneme->prior->name)
        //          << phoneme->name << "next"
        //          << (phoneme->next == nullptr ? "null" : phoneme->next->name);
    }

    // Draw playback indicator
    painter.setRenderHint(QPainter::Antialiasing);
    penWidth = 1.0;
    pen.setWidthF(penWidth);
    pen.setColor(positionLineColor);
    painter.setPen(pen);
    auto x = tickToX(m_position);
    painter.drawLine(QLineF(x, 0, x, rect().height()));
}
void PhonemeView::drawBar(QPainter *painter, int tick, int bar) {
    QPen pen;
    auto x = tickToX(tick); // tick to itemX
    pen.setColor(barLineColor);
    painter->setPen(pen);
    auto y1 = 0;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void PhonemeView::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(beatLineColor);
    painter->setPen(pen);
    auto y1 = 0;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void PhonemeView::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(commonLineColor);
    painter->setPen(pen);
    auto y1 = 0;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void PhonemeView::mousePressEvent(QMouseEvent *event) {
    if (!canEdit())
        return;

    m_mouseDownX = event->pos().x();
    auto tick = xToTick(event->pos().x());
    if (auto phoneme = phonemeAtTick(tick)) {
        m_freezeHoverEffects = true;
        m_curPhoneme = phoneme;
        auto phonemeStart = phoneme->start;
        auto phonemeEnd = phoneme->end();
        if (tick - phonemeStart < m_resizeToleranceInTick && phoneme->canResizeLeft()) {
            m_mouseMoveBehavior = ResizeLeft;
            m_curOtherPhoneme = phoneme->prior;
            qDebug() << "PhonemeView: About to resize left:"
                     << "prior:" << phoneme->prior->name << "current:" << phoneme->name;
        } else if (phonemeEnd - tick < m_resizeToleranceInTick && phoneme->canResizeRight()) {
            m_mouseMoveBehavior = ResizeRight;
            if (auto next = m_curPhoneme->next) {
                m_curOtherPhoneme = next;
                qDebug() << "PhonemeView: About to resize right:"
                         << "current:" << phoneme->name << "next:" << next->name;
            } else {
                m_curOtherPhoneme = nullptr;
                qDebug() << "PhonemeView: About to resize right:"
                         << "current:" << phoneme->name;
            }
        } else {
            m_mouseMoveBehavior = None;
        }
    } else
        QWidget::mousePressEvent(event);
}
void PhonemeView::mouseMoveEvent(QMouseEvent *event) {
    if (!canEdit())
        return;

    auto deltaX = event->pos().x() - m_mouseDownX;
    auto deltaTick = qRound(xToTick(event->pos().x()) - xToTick(m_mouseDownX));
    if (m_mouseMoveBehavior == ResizeLeft) {
        // qDebug() << deltaX << deltaTick;
        auto prior = m_curOtherPhoneme;
        auto priorEnd = prior->end();
        int priorTargetLength;
        auto curTargetStart = m_curPhoneme->start + deltaTick;
        auto curTargetLength = m_curPhoneme->length - deltaTick;
        priorTargetLength = curTargetStart - prior->start;
        if (priorTargetLength <= 0) {
            qDebug() << "priorTargetLength <= 0";
            deltaTick = -(m_curPhoneme->start - prior->start);
            m_curOtherPhoneme->lengthOffset =
                m_curPhoneme->start + deltaTick - m_curOtherPhoneme->end();
        } else {
            m_curOtherPhoneme->lengthOffset = curTargetStart - priorEnd;
        }
        if (curTargetLength <= 0) {
            deltaTick = m_curPhoneme->length;
            m_curOtherPhoneme->lengthOffset = deltaTick;
        }
        m_curPhoneme->startOffset = deltaTick;
        m_curPhoneme->lengthOffset = -deltaTick;
    } else if (m_mouseMoveBehavior == ResizeRight) {
        auto next = m_curOtherPhoneme;
        auto nextStart = next->start;
        auto curTargetLength = m_curPhoneme->length + deltaTick;
        if (curTargetLength <= 0) {
            deltaTick = m_curPhoneme->start - next->start;
        }
        auto nextTargetLength = next->length - deltaTick;
        if (nextTargetLength <= 0) {
            // qDebug() << "nextTargetLength <= 0" << nextTargetLength;
            deltaTick = next->length;
        }
        m_curPhoneme->lengthOffset = deltaTick;
        m_curOtherPhoneme->startOffset = deltaTick;
        m_curOtherPhoneme->lengthOffset = -deltaTick;
    }

    update();
    // QWidget::mouseMoveEvent(event);
}
void PhonemeView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouseMoveBehavior == ResizeLeft)
        emit adjustCompleted(m_curPhoneme);
    else if (m_mouseMoveBehavior == ResizeRight)
        emit adjustCompleted(m_curOtherPhoneme);

    if (m_curPhoneme) {
        m_curPhoneme->startOffset = 0;
        m_curPhoneme->lengthOffset = 0;
        m_curPhoneme = nullptr;
    }
    if (m_curOtherPhoneme) {
        m_curOtherPhoneme->startOffset = 0;
        m_curOtherPhoneme->lengthOffset = 0;
        m_curOtherPhoneme = nullptr;
    }
    m_mouseMoveBehavior = None;
    m_freezeHoverEffects = false;
    updateHoverEffects();
    QWidget::mouseReleaseEvent(event);
}
void PhonemeView::updateHoverEffects() {
    auto pos = mapFromGlobal(QCursor::pos());
    auto tick = xToTick(pos.x());
    if (auto phoneme = phonemeAtTick(tick)) {
        auto phonemeStart = phoneme->start + phoneme->startOffset;
        auto phonemeEnd = phoneme->endWithOffset();
        if (tick - phonemeStart < m_resizeToleranceInTick && phoneme->canResizeLeft()) {
            setCursor(Qt::SizeHorCursor);
            phoneme->hoverOnControlBar = true;
            clearHoverEffects(phoneme);
            update();
        } else if (phonemeEnd - tick < m_resizeToleranceInTick && phoneme->canResizeRight()) {
            setCursor(Qt::SizeHorCursor);
            phoneme->hoverOnControlBar = false;
            if (auto next = phoneme->next) {
                next->hoverOnControlBar = true;
                clearHoverEffects(next);
            }
            update();
        } else {
            phoneme->hoverOnControlBar = false;
            if (auto next = phoneme->next)
                next->hoverOnControlBar = false;
            setCursor(Qt::ArrowCursor);
            clearHoverEffects();
            update();
        }
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
double PhonemeView::tickToX(double tick) {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = (rect().width() - verticalScrollBarWidth) * ratio;
    return x;
}
double PhonemeView::xToTick(double x) {
    auto tick = 1.0 * x / (rect().width() - verticalScrollBarWidth) * (m_endTick - m_startTick) +
                m_startTick;
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
PhonemeView::NoteViewModel *PhonemeView::findNoteById(int id) {
    for (const auto note : m_notes)
        if (note->id == id)
            return note;
    return nullptr;
}
PhonemeView::PhonemeViewModel *PhonemeView::phonemeAtTick(double tick) {
    for (const auto phoneme : m_phonemes) {
        if (phoneme->start < tick && phoneme->end() > tick)
            return phoneme;
    }
    return nullptr;
}
void PhonemeView::buildPhonemeList() {
    auto appModel = AppModel::instance();

    PhonemeViewModel *prior;
    NoteViewModel *priorNote = nullptr;

    auto padStartPhoneme = new PhonemeViewModel;
    padStartPhoneme->noteId = -1;
    padStartPhoneme->type = PhonemeViewModel::Sil;
    padStartPhoneme->start = -1920;
    padStartPhoneme->prior = nullptr;
    padStartPhoneme->next = nullptr;
    // padStartPhoneme->name = "sil";
    prior = padStartPhoneme;
    m_phonemes.append(padStartPhoneme);

    auto insertNextNode = [](PhonemeViewModel *p1, PhonemeViewModel *p2) {
        p2->next = p1->next;
        if (p1->next != nullptr)
            p1->next->prior = p2;
        p2->prior = p1;
        p1->next = p2;
    };

    for (const auto note : m_notes) {
        for (const auto &phoneme : note->editedPhonemes) {
            auto phonemeViewModel = new PhonemeViewModel;
            auto noteStartMs = appModel->tickToMs(note->start);
            if (phoneme.type == Phoneme::Ahead) {
                phonemeViewModel->noteId = note->id;
                phonemeViewModel->type = PhonemeViewModel::Ahead;
                auto phoneStartMs = noteStartMs - phoneme.start;
                auto phoneStartTick = qRound(appModel->msToTick(phoneStartMs));
                phonemeViewModel->start = phoneStartTick;
                phonemeViewModel->name = phoneme.name;
                if (priorNote == nullptr) {
                    prior->length = phoneStartTick - prior->start;
                } else {
                    int priorNoteEnd = priorNote->start + priorNote->length;
                    if (phoneStartTick < priorNoteEnd) {
                        prior->length = phoneStartTick - prior->start;
                    } else {
                        prior->length = priorNoteEnd - prior->start;

                        auto padPhonemeViewModel = new PhonemeViewModel;
                        padPhonemeViewModel->noteId = -1;
                        padPhonemeViewModel->type = PhonemeViewModel::Sil;
                        // padPhonemeViewModel->name = "sil";
                        padPhonemeViewModel->start = priorNoteEnd;
                        padPhonemeViewModel->length = phoneStartTick - priorNoteEnd;
                        m_phonemes.append(padPhonemeViewModel);
                        insertNextNode(prior, padPhonemeViewModel);

                        prior = padPhonemeViewModel;
                    }
                }
            } else if (phoneme.type == Phoneme::Normal) {
                phonemeViewModel->noteId = note->id;
                phonemeViewModel->type = PhonemeViewModel::Normal;
                phonemeViewModel->name = phoneme.name;
                auto phoneStartMs = noteStartMs + phoneme.start;
                auto phoneStartTick = qRound(appModel->msToTick(phoneStartMs));
                phonemeViewModel->start = phoneStartTick;
                prior->length = phoneStartTick - prior->start;
                // if (priorNote == nullptr) {
                //     prior->length = phoneStartTick - prior->start;
                // } else {
                //     int priorNoteEnd = priorNote->start + priorNote->length;
                //
                //     if (prior->noteId == priorNote->id) {
                //         if (phoneStartTick < priorNoteEnd) {
                //             prior->length = phoneStartTick - prior->start;
                //         } else
                //             prior->length = priorNoteEnd - prior->start;
                //     } else
                //         prior->length = phoneStartTick - prior->start;
                // }
            }
            m_phonemes.append(phonemeViewModel);
            insertNextNode(prior, phonemeViewModel);
            prior = phonemeViewModel;
        }
        priorNote = note;
    }

    auto lastNote = m_notes.at(m_notes.count() - 1);
    auto lastPhoneme = m_phonemes.last();
    lastPhoneme->length = lastNote->start + lastNote->length - lastPhoneme->start;

    // auto padEndPhoneme = new PhonemeViewModel;
    // padEndPhoneme->noteId = -1;
    // padEndPhoneme->type = PhonemeViewModel::Sil;
    // padEndPhoneme->start = -1920;
    // padEndPhoneme->next = nullptr;
    // padEndPhoneme->length = 1920;
    // m_phonemes.append(padEndPhoneme);
    // insertNextNode(prior, padEndPhoneme);
}
void PhonemeView::resetPhonemeList() {
    for (auto phoneme : m_phonemes)
        delete phoneme;
    m_phonemes.clear();
}
void PhonemeView::clearHoverEffects(PhonemeViewModel *except) {
    for (const auto item : m_phonemes) {
        if (item != except && item->hoverOnControlBar)
            item->hoverOnControlBar = false;
    }
}