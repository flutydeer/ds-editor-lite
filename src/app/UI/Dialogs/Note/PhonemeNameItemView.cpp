//
// Created by FlutyDeer on 2026/4/1.
//

#include "PhonemeNameItemView.h"

#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QHBoxLayout>

PhonemeNameItemView::PhonemeNameItemView(QWidget *parent) : QWidget(parent) {
    m_cbLanguage = new LanguageComboBox({});
    m_leName = new LineEdit;
    m_cbIsOnset = new QCheckBox;
    m_cbIsOnset->setText(tr("Onset"));

    auto layout = new QHBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(8);
    layout->addWidget(m_cbLanguage);
    layout->addWidget(m_leName);
    layout->addWidget(m_cbIsOnset);
    layout->addStretch();
    setLayout(layout);
    setContentsMargins({});
}

LanguageComboBox *PhonemeNameItemView::cbLanguage() const {
    return m_cbLanguage;
}

LineEdit *PhonemeNameItemView::leName() const {
    return m_leName;
}

QCheckBox *PhonemeNameItemView::cbIsOnset() const {
    return m_cbIsOnset;
}
