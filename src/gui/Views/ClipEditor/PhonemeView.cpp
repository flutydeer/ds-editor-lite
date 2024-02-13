//
// Created by fluty on 24-2-12.
//

#include "PhonemeView.h"

#include "Model/AppModel.h"
#include "Utils/AppGlobal.h"

using namespace AppGlobal;

void PhonemeView::insertNote(Note *note) {
    auto noteViewModel = new NoteViewModel;
    noteViewModel->id = note->id();
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
    noteViewModel->originalPhonemes = note->phonemes().original;
    noteViewModel->editedPhonemes = note->phonemes().edited;
    m_notes.append(noteViewModel);
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::removeNote(int noteId) {
    qDebug() << "PhonemeView::removeNote" << noteId;
    auto note = findNoteById(noteId);
    m_notes.removeOne(note);
    delete note;
    resetPhonemeList();
    buildPhonemeList();
    update();
}
void PhonemeView::updateNote(Note *note) {
    auto noteViewModel = findNoteById(note->id());
    noteViewModel->start = note->start();
    noteViewModel->length = note->length();
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

    auto drawSolidLine = [&](double tick) {
        auto x = tickToX(tick);
        pen.setColor(mainColor);
        painter.setPen(pen);
        painter.drawLine(QLineF(x, 0, x, rect().height()));
    };

    auto drawPhoneName = [&](double startTick, const QString &name) {
        auto start = tickToX(startTick);
        // auto length = tickToX(endTick) - start;
        auto length = 80;
        auto textRect = QRectF(start + 4, 0, length, rect().height());
        painter.setPen(mainColor);
        painter.setBrush(fillColor);
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);
        painter.drawText(textRect, name, textOption);
    };

    for (const auto phoneme : m_phonemes) {
        if (phoneme->start + phoneme->length < m_startTick)
            continue;
        if (phoneme->start > m_endTick)
            break;
        if (phoneme->type == PhonemeViewModel::Sil)
            continue;

        drawSolidRect(phoneme->start, phoneme->start + phoneme->length);
        drawSolidLine(phoneme->start);
        drawPhoneName(phoneme->start, phoneme->name);
    }

    // Draw playback indicator
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
double PhonemeView::tickToX(double tick) {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = qRound((rect().width() - verticalScrollBarWidth) * ratio);
    return x;
}
double PhonemeView::xToTick(double x) {
    auto tick = 1.0 * x / (rect().width() - verticalScrollBarWidth) * (m_endTick - m_startTick) +
                m_startTick;
    if (tick < 0)
        tick = 0;
    return tick;
}
PhonemeView::NoteViewModel *PhonemeView::findNoteById(int id) {
    for (const auto note : m_notes)
        if (note->id == id)
            return note;
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
            }
            m_phonemes.append(phonemeViewModel);
            insertNextNode(prior, phonemeViewModel);
            prior = phonemeViewModel;
        }
        priorNote = note;
    }

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