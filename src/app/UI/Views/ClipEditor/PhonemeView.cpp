//
// Created by fluty on 24-2-12.
//

#include "PhonemeView.h"

#include <QMouseEvent>
#include <QPainter>

#include "Model/AppModel.h"
#include "Global/AppGlobal.h"
#include "Model/Note.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"

using namespace AppGlobal;

PhonemeView::PhonemeView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("PhonemeView");
    installEventFilter(this);

    connect(appModel, &AppModel::modelChanged, this, [=] {
        setTimeSignature(appModel->timeSignature().numerator,
                         appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, this, &PhonemeView::setTimeSignature);
    connect(appModel, &AppModel::quantizeChanged, this, &PhonemeView::setQuantize);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &PhonemeView::setPosition);
}
void PhonemeView::setSingingClip(SingingClip *singingClip) {
    m_singingClip = singingClip;
}
void PhonemeView::insertNote(Note *note) {
    auto noteViewModel = new NoteViewModel;
    noteViewModel->id = note->id();
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
    noteViewModel->isSlur = note->isSlur();
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
void PhonemeView::updateNoteTime(Note *note) {
    qDebug() << "PhonemeView::updateNoteTime" << note->id() << note->lyric()
             << note->pronunciation().original;
    auto noteViewModel = findNoteById(note->id());
    m_notes.remove(noteViewModel);
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
    m_notes.add(noteViewModel);
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::updateNotePhonemes(Note *note) {
    auto noteViewModel = findNoteById(note->id());
    noteViewModel->originalPhonemes = note->phonemes().original;
    noteViewModel->editedPhonemes = note->phonemes().edited;
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

    auto mainColor = QColor(155, 186, 255);
    auto fillColor = QColor(155, 186, 255, 50);
    auto positionLineColor = QColor(255, 204, 153);
    // auto noteBoundaryColor = QColor(100, 100, 100);
    // Draw background
    // painter.setPen(Qt::NoPen);
    // painter.setBrush(QColor(28, 29, 30));
    // painter.drawRect(rect());
    // painter.setBrush(Qt::NoBrush);

    // Draw graduates
    drawTimeline(&painter, m_startTick, m_endTick,
                 rect().width() - AppGlobal::verticalScrollBarWidth);

    auto drawSolidRect = [&](double startTick, double endTick, const QColor &color) {
        auto start = tickToX(startTick);
        auto length = tickToX(endTick) - start;
        auto rectF = QRectF(start, 0, length, rect().height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRect(rectF);
    };

    auto drawSolidLine = [&](double tick, double penWidth, const QColor &color) {
        auto x = tickToX(tick);
        pen.setColor(color);
        pen.setWidthF(penWidth);
        painter.setPen(pen);
        painter.drawLine(QLineF(x, 0, x, rect().height()));
    };

    auto drawPhoneName = [&](PhonemeViewModel *phoneme) {
        auto start = tickToX(phoneme->start + phoneme->startOffset);
        auto length = 80;
        auto textRect = QRectF(start + 2, 0, length - 4, rect().height());
        painter.setPen(mainColor);
        painter.setBrush(fillColor);

        QString text;
        if (m_showDebugInfo)
            text = phoneme->name +
                   QString(" s%1 so%2 l%3 lo%4").arg(phoneme->start).arg(phoneme->startOffset);
        else
            text = phoneme->name;
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);
        painter.drawText(textRect, text, textOption);
    };

    // Draw notes' word boundary
    for (int i = 0; i < m_notes.count(); i++) {
        auto curNote = m_notes.at(i);
        if (curNote->end() < m_startTick)
            continue;
        if (curNote->start > m_endTick)
            break;

        // if (!curNote->isSlur)
        //     drawSolidLine(curNote->start, 1, noteBoundaryColor);
        //
        // if (i < m_notes.count() - 1) {
        //     auto nextNote = m_notes.at(i + 1);
        //     if (!nextNote->isSlur)
        //         drawSolidLine(curNote->end(), 1, noteBoundaryColor);
        // } else
        //     drawSolidLine(curNote->end(), 1, noteBoundaryColor);

        if (canEdit())
            painter.setRenderHint(QPainter::Antialiasing, false);
        drawSolidRect(curNote->start, curNote->end(), fillColor);
    }

    if (canEdit()) {
        // TODO： use binary find
        for (const auto phoneme : m_phonemes) {
            if (phoneme->start < m_startTick)
                continue;
            if (phoneme->start > m_endTick)
                break;
            if (phoneme->type == PhonemeViewModel::Sil)
                continue;

            auto start = phoneme->start + phoneme->startOffset;
            auto phonemePenWidth = phoneme->hoverOnControlBar ? 2.5 : 1.0;
            painter.setRenderHint(QPainter::Antialiasing);
            drawSolidLine(start, phonemePenWidth, mainColor);
            drawPhoneName(phoneme);
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
    if (!canEdit()) {
        event->ignore();
        return;
    }

    m_mouseDownX = event->pos().x();
    auto tick = xToTick(event->pos().x());
    if (auto phoneme = phonemeAtTick(tick)) {
        m_freezeHoverEffects = true;
        m_curPhoneme = phoneme;
        m_mouseMoveBehavior = Move;
    } else {
        QWidget::mousePressEvent(event);
        event->ignore();
    }
}
void PhonemeView::mouseMoveEvent(QMouseEvent *event) {
    if (!canEdit())
        return;

    auto deltaTick = qRound(xToTick(event->pos().x()) - xToTick(m_mouseDownX));
    if (m_mouseMoveBehavior == Move) {
        auto cur = m_curPhoneme;
        auto prior = cur->prior;
        auto curTargetStart = cur->start + deltaTick;
        if (curTargetStart <= prior->start)
            deltaTick = prior->start - cur->start;
        if (auto next = cur->next) {
            auto nextStart = next->start;
            if (curTargetStart > nextStart)
                deltaTick = nextStart - cur->start;
        }

        cur->startOffset = deltaTick;
    }

    update();
}
void PhonemeView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouseMoveBehavior == Move)
        handleAdjustCompleted(m_curPhoneme);

    if (m_curPhoneme) {
        m_curPhoneme->startOffset = 0;
        m_curPhoneme = nullptr;
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
        if (qAbs(tick - phoneme->start) < m_resizeToleranceInTick)
            return phoneme;
    }
    return nullptr;
}
void PhonemeView::buildPhonemeList() {
    if (m_notes.count() == 0)
        return;

    auto head = new PhonemeViewModel;
    head->noteId = -1;
    head->type = PhonemeViewModel::Sil;
    head->start = -1920;
    head->prior = nullptr;
    head->next = nullptr;
    PhonemeViewModel *prior = head;
    m_phonemes.append(head);

    auto insertNextNode = [](PhonemeViewModel *p1, PhonemeViewModel *p2) {
        p2->next = p1->next;
        if (p1->next != nullptr)
            p1->next->prior = p2;
        p2->prior = p1;
        p1->next = p2;
    };

    for (const auto note : m_notes) {
        if (note->isSlur)
            continue;

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
            } else if (phoneme.type == Phoneme::Normal) {
                phonemeViewModel->noteId = note->id;
                phonemeViewModel->type = PhonemeViewModel::Normal;
                phonemeViewModel->name = phoneme.name;
                auto phoneStartMs = noteStartMs + phoneme.start;
                auto phoneStartTick = qRound(appModel->msToTick(phoneStartMs));
                phonemeViewModel->start = phoneStartTick;
            }
            m_phonemes.append(phonemeViewModel);
            insertNextNode(prior, phonemeViewModel);
            prior = phonemeViewModel;
        }
    }
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
void PhonemeView::handleAdjustCompleted(PhonemeViewModel *phonemeViewModel) {
    if (!phonemeViewModel)
        return;

    QList<int> notesId;
    QList<Phoneme> phonemes;
    notesId.append(phonemeViewModel->noteId);
    auto note = m_singingClip->findNoteById(phonemeViewModel->noteId);
    Phoneme phoneme;
    phoneme.name = phonemeViewModel->name;
    auto noteStartInMs = appModel->tickToMs(note->start());
    auto phonemeViewModelStartInMs =
        appModel->tickToMs(phonemeViewModel->start + phonemeViewModel->startOffset);
    if (phonemeViewModel->type == PhonemeView::PhonemeViewModel::Ahead) {
        phoneme.type = Phoneme::Ahead;
        phoneme.start = qRound(noteStartInMs - phonemeViewModelStartInMs);
        qDebug() << "ClipEditorView::onAdjustPhonemeCompleted"
                 << "append ahead" << phoneme.name << phoneme.start;
    } else if (phonemeViewModel->type == PhonemeView::PhonemeViewModel::Normal) {
        phoneme.type = Phoneme::Normal;
        phoneme.start = qRound(phonemeViewModelStartInMs - noteStartInMs);
        qDebug() << "ClipEditorView::onAdjustPhonemeCompleted"
                 << "append normal" << phoneme.name << phoneme.start;
    }
    phonemes.append(phoneme);
    clipController->onAdjustPhoneme(notesId, phonemes);
}