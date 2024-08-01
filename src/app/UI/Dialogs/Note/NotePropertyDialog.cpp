//
// Created by fluty on 24-8-1.
//

#include "NotePropertyDialog.h"

#include "Global/ClipEditorGlobal.h"
#include "Model/Note.h"
#include "UI/Controls/LineEdit.h"

#include <QFormLayout>
#include <QLabel>

NotePropertyDialog::NotePropertyDialog(Note *note, QWidget *parent)
    : OKCancelDialog(parent), m_note() {
    setWindowTitle(tr("Properties of note \"%1\"").arg(note->lyric()));
    setFocusPolicy(Qt::ClickFocus);

    // TODO：用 ComboBox 选择语言
    auto leLanguage = new QLineEdit(note->language());

    auto leLyric = new QLineEdit(note->lyric());
    leLyric->setPlaceholderText(ClipEditorGlobal::defaultLyric);

    auto lePron = new QLineEdit(note->pronunciation().edited);
    lePron->setPlaceholderText(note->pronunciation().original);

    auto phoneme = note->phonemes();
    auto originalPhoneme = phoneme.original;
    auto editedPhoneme = phoneme.edited;

    QList<Phoneme> originalAhead;
    QList<Phoneme> originalNormal;
    QList<Phoneme> originalFinal;
    regroupPhonemes(originalPhoneme, originalAhead, originalNormal, originalFinal);

    QList<Phoneme> editedAhead;
    QList<Phoneme> editedNormal;
    QList<Phoneme> editedFinal;
    regroupPhonemes(editedPhoneme, editedAhead, editedNormal, editedFinal);

    auto lePhonemeAhead = new QLineEdit(phonemesToString(editedAhead));
    lePhonemeAhead->setPlaceholderText(phonemesToString(originalAhead));

    auto lePhonemeNormal = new QLineEdit(phonemesToString(editedNormal));
    lePhonemeNormal->setPlaceholderText(phonemesToString(originalNormal));

    auto lePhonemeFinal = new QLineEdit(phonemesToString(editedFinal));
    lePhonemeFinal->setPlaceholderText(phonemesToString(originalFinal));

    auto mainLayout = new QFormLayout;
    mainLayout->addRow(tr("Language:"), leLanguage);
    mainLayout->addRow(tr("Lyric:"), leLyric);
    mainLayout->addRow(tr("Pronunciation:"), lePron);
    mainLayout->addRow(tr("Phonemes Ahead:"), lePhonemeAhead);
    mainLayout->addRow(tr("Phonemes Normal:"), lePhonemeNormal);
    mainLayout->addRow(tr("Phonemes Final:"), lePhonemeFinal);
    mainLayout->setContentsMargins({});

    // auto mainLayout = new QVBoxLayout();
    // mainLayout->addWidget(leLanguage);
    // mainLayout->addWidget(leLyric);
    // mainLayout->addWidget(lePron);
    // mainLayout->addWidget(lePhonemeAhead);
    // mainLayout->addWidget(lePhonemeNormal);
    // mainLayout->addWidget(lePhonemeFinal);

    body()->setLayout(mainLayout);

    setModal(true);
}
void NotePropertyDialog::regroupPhonemes(const QList<Phoneme> &phonemes, QList<Phoneme> &ahead,
                                         QList<Phoneme> &normal, QList<Phoneme> &final) {
    for (const Phoneme &phoneme : phonemes) {
        if (phoneme.type == Phoneme::Ahead)
            ahead.append(phoneme);
        else if (phoneme.type == Phoneme::Normal)
            normal.append(phoneme);
        else if (phoneme.type == Phoneme::Final)
            final.append(phoneme);
    }
}
QString NotePropertyDialog::phonemesToString(const QList<Phoneme> &phonemes) {
    QString result;
    int i = 0;
    for (const Phoneme &phoneme : phonemes) {
        result.append(phoneme.name);
        if (i < phonemes.count() - 1)
            result.append(" ");
        i++;
    }
    return result;
}