#include "MidiBatchImportDialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <lite/ProjectConverters/MidiTextCodecConverter.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>

class MidiBatchImportDialogPrivate {
    Q_DECLARE_PUBLIC(MidiBatchImportDialog)
public:
    explicit MidiBatchImportDialogPrivate(MidiBatchImportDialog *q, const QByteArray &defaultCodec)
        : q_ptr(q), defaultCodec(defaultCodec) {
    }

    void init() {
        Q_Q(MidiBatchImportDialog);

        auto *contentLayout = new QVBoxLayout(q->body());

        auto *codecLayout = new QHBoxLayout;
        auto *codecLabel = new QLabel(MidiBatchImportDialog::tr("Encoding:"), q);
        codecComboBox = new ComboBox(q);
        codecLayout->addWidget(codecLabel);
        codecLayout->addWidget(codecComboBox, 1);
        contentLayout->addLayout(codecLayout);

        const auto codecs = MidiTextCodecConverter::availableCodecs();
        for (const auto &codec : codecs) {
            codecComboBox->addItem(codec.displayName, codec.identifier);
            if (codec.identifier == defaultCodec)
                codecComboBox->setCurrentIndex(codecComboBox->count() - 1);
        }

        importTempoCheckBox = new QCheckBox(MidiBatchImportDialog::tr("Import tempo"), q);
        importTempoCheckBox->setChecked(true);
        contentLayout->addWidget(importTempoCheckBox);

        importTimeSignatureCheckBox =
            new QCheckBox(MidiBatchImportDialog::tr("Import time signature"), q);
        importTimeSignatureCheckBox->setChecked(true);
        contentLayout->addWidget(importTimeSignatureCheckBox);

        auto *okButton = new AccentButton(MidiBatchImportDialog::tr("OK"));
        q->setPositiveButton(okButton);
        auto *cancelButton = new Button(MidiBatchImportDialog::tr("Cancel"));
        q->setNegativeButton(cancelButton);

        QObject::connect(okButton, &QAbstractButton::clicked, q, &Dialog::accept);
        QObject::connect(cancelButton, &QAbstractButton::clicked, q, &Dialog::reject);

        q->resize(360, 200);
    }

    MidiBatchImportDialog *q_ptr;
    QByteArray defaultCodec;
    ComboBox *codecComboBox = nullptr;
    QCheckBox *importTempoCheckBox = nullptr;
    QCheckBox *importTimeSignatureCheckBox = nullptr;
};

MidiBatchImportDialog::MidiBatchImportDialog(const QByteArray &defaultCodec, QWidget *parent)
    : Dialog(parent), d_ptr(new MidiBatchImportDialogPrivate(this, defaultCodec)) {
    Q_D(MidiBatchImportDialog);
    d->init();
}

MidiBatchImportDialog::~MidiBatchImportDialog() = default;

QByteArray MidiBatchImportDialog::selectedCodec() const {
    Q_D(const MidiBatchImportDialog);
    if (!d->codecComboBox || d->codecComboBox->currentIndex() < 0)
        return {};
    return d->codecComboBox->currentData().toByteArray();
}

bool MidiBatchImportDialog::importTempo() const {
    Q_D(const MidiBatchImportDialog);
    return d->importTempoCheckBox && d->importTempoCheckBox->isChecked();
}

bool MidiBatchImportDialog::importTimeSignature() const {
    Q_D(const MidiBatchImportDialog);
    return d->importTimeSignatureCheckBox && d->importTimeSignatureCheckBox->isChecked();
}
