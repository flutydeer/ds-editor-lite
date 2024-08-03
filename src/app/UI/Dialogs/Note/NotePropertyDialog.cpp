//
// Created by fluty on 24-8-1.
//

#include "NotePropertyDialog.h"

#include "Global/ClipEditorGlobal.h"
#include "Model/Note.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "Utils/Linq.h"

#include <QSpinBox>
#include <QFormLayout>

NotePropertyDialog::NotePropertyDialog(Note *note, QWidget *parent)
    : OKCancelDialog(parent), m_note() {
    setWindowTitle(tr("Note Properties - %1").arg(note->lyric()));
    setFocusPolicy(Qt::ClickFocus);

    auto sbStart = new QSpinBox;
    sbStart->setMinimum(0);
    sbStart->setMaximum(INT_MAX);
    sbStart->setValue(note->start());
    sbStart->setSingleStep(5);

    auto sbLength = new QSpinBox;
    sbLength->setMinimum(0);
    sbLength->setMaximum(INT_MAX);
    sbLength->setValue(note->length());
    sbLength->setSingleStep(5);

    auto cbLanguage = new LanguageComboBox(note->language());

    auto leLyric = new QLineEdit(note->lyric());
    leLyric->setPlaceholderText(note->lyric());

    auto lePron = new QLineEdit(note->pronunciation().edited);
    lePron->setPlaceholderText(note->pronunciation().original);

    auto phoneme = note->phonemes();
    auto originalPhoneme = phoneme.original;
    auto editedPhoneme = phoneme.edited;

    auto types = Phoneme::phonemesTypes();
    auto getter = [](const Phoneme &phoneme) { return phoneme.type; };
    const auto originalMap = Linq::groupBy(originalPhoneme, types, getter);
    const auto editedMap = Linq::groupBy(editedPhoneme, types, getter);

    auto lePhonemeAhead = new QLineEdit(phonemesToString(editedMap[Phoneme::Ahead]));
    lePhonemeAhead->setPlaceholderText(phonemesToString(originalMap[Phoneme::Ahead]));

    auto lePhonemeNormal = new QLineEdit(phonemesToString(editedMap[Phoneme::Normal]));
    lePhonemeNormal->setPlaceholderText(phonemesToString(originalMap[Phoneme::Normal]));

    auto lePhonemeFinal = new QLineEdit(phonemesToString(editedMap[Phoneme::Final]));
    lePhonemeFinal->setPlaceholderText(phonemesToString(originalMap[Phoneme::Final]));

    auto mainLayout = new QFormLayout;
    mainLayout->setLabelAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignTrailing |
                                  Qt::AlignmentFlag::AlignVCenter);
    mainLayout->addRow(tr("Start:"), sbStart);
    mainLayout->addRow(tr("Length:"), sbLength);
    mainLayout->addRow(tr("Language:"), cbLanguage);
    mainLayout->addRow(tr("Lyric:"), leLyric);
    mainLayout->addRow(tr("Pronunciation:"), lePron);
    mainLayout->addRow(tr("Ahead Phonemes:"), lePhonemeAhead);
    mainLayout->addRow(tr("Normal Phonemes:"), lePhonemeNormal);
    mainLayout->addRow(tr("Final Phonemes:"), lePhonemeFinal);
    mainLayout->setContentsMargins({});

    body()->setLayout(mainLayout);

    setModal(true);
    connect(okButton(), &AccentButton::clicked, this, &Dialog::accept);
    connect(cancelButton(), &AccentButton::clicked, this, &Dialog::reject);
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
QList<Phoneme> NotePropertyDialog::phonemesFromString(Phoneme::PhonemeType type,
                                                      const QString &names) {
    QList<Phoneme> result;
    for (const auto &item : names.split(" ")) {
        Phoneme phoneme;
        phoneme.type = type;
        phoneme.name = item;
        result.append(phoneme);
    }
    return result;
}