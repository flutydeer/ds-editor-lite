//
// Created by fluty on 24-8-1.
//

#include "NotePropertyDialog.h"

#include "Global/AppGlobal.h"
#include "Global/ClipEditorGlobal.h"
#include "Model/AppModel/Note.h"
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

    // auto sbStart = new QSpinBox;
    // sbStart->setMinimum(0);
    // sbStart->setMaximum(INT_MAX);
    // sbStart->setValue(note->start());
    // sbStart->setSingleStep(5);
    //
    // auto sbLength = new QSpinBox;
    // sbLength->setMinimum(0);
    // sbLength->setMaximum(INT_MAX);
    // sbLength->setValue(note->length());
    // sbLength->setSingleStep(5);

    m_cbLanguage = new LanguageComboBox(note->language());

    m_leLyric = new QLineEdit(note->lyric());
    m_leLyric->setPlaceholderText(note->lyric());

    m_result.pronunciation = note->pronunciation();
    m_lePron = new QLineEdit(note->pronunciation().edited);
    m_lePron->setPlaceholderText(note->pronunciation().original);

    m_result.phonemes = note->phonemeInfo();
    auto phoneme = note->phonemeInfo();
    auto originalPhoneme = phoneme.original;
    auto editedPhoneme = phoneme.edited;

    auto types = Phoneme::phonemesTypes();
    auto getter = [](const Phoneme &phoneme) { return phoneme.type; };
    const auto originalMap = Linq::groupBy(originalPhoneme, types, getter);
    const auto editedMap = Linq::groupBy(editedPhoneme, types, getter);

    m_lePhonemeAhead = new QLineEdit(phonemesToString(editedMap[Phoneme::Ahead]));
    m_lePhonemeAhead->setPlaceholderText(phonemesToString(originalMap[Phoneme::Ahead]));

    m_lePhonemeNormal = new QLineEdit(phonemesToString(editedMap[Phoneme::Normal]));
    m_lePhonemeNormal->setPlaceholderText(phonemesToString(originalMap[Phoneme::Normal]));

    m_lePhonemeFinal = new QLineEdit(phonemesToString(editedMap[Phoneme::Final]));
    m_lePhonemeFinal->setPlaceholderText(phonemesToString(originalMap[Phoneme::Final]));

    auto mainLayout = new QFormLayout;
    mainLayout->setLabelAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignTrailing |
                                  Qt::AlignmentFlag::AlignVCenter);
    // mainLayout->addRow(tr("Start:"), sbStart);
    // mainLayout->addRow(tr("Length:"), sbLength);
    mainLayout->addRow(tr("Language:"), m_cbLanguage);
    mainLayout->addRow(tr("Lyric:"), m_leLyric);
    mainLayout->addRow(tr("Pronunciation:"), m_lePron);
    mainLayout->addRow(tr("Ahead Phonemes:"), m_lePhonemeAhead);
    mainLayout->addRow(tr("Normal Phonemes:"), m_lePhonemeNormal);
    mainLayout->addRow(tr("Final Phonemes:"), m_lePhonemeFinal);
    mainLayout->setContentsMargins({});

    body()->setLayout(mainLayout);

    setModal(true);
    connect(okButton(), &AccentButton::clicked, this, &Dialog::accept);
    connect(cancelButton(), &AccentButton::clicked, this, &Dialog::reject);
}

NoteDialogResult NotePropertyDialog::result() {
    m_result.language =
        languageKeyFromType(static_cast<AppGlobal::LanguageType>(m_cbLanguage->currentIndex()));
    m_result.lyric = m_leLyric->text();
    m_result.pronunciation.edited = m_lePron->text();
    m_result.phonemes.edited.clear();

    auto aheadText = m_lePhonemeAhead->text();
    if (!aheadText.isNull() && !aheadText.isEmpty()) {
        auto aheadList = phonemesFromString(Phoneme::Ahead, m_lePhonemeAhead->text());
        if (!aheadList.isEmpty())
            m_result.phonemes.edited += aheadList;
    }

    auto normalText = m_lePhonemeNormal->text();
    if (!normalText.isNull() && !normalText.isEmpty()) {
        auto normalList = phonemesFromString(Phoneme::Normal, m_lePhonemeNormal->text());
        if (!normalList.isEmpty())
            m_result.phonemes.edited += normalList;
    }
    auto finalText = m_lePhonemeFinal->text();
    if (!finalText.isNull() && !finalText.isEmpty()) {
        auto finalList = phonemesFromString(Phoneme::Final, m_lePhonemeFinal->text());
        if (!finalList.isEmpty())
            m_result.phonemes.edited += finalList;
    }

    return m_result;
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
    auto strList = names.split(" ");
    for (const auto &item : strList) {
        Phoneme phoneme;
        phoneme.type = type;
        phoneme.name = item;
        result.append(phoneme);
    }
    return result;
}