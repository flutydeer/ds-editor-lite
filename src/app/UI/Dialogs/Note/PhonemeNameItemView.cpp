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
    m_leName->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_leName->setMaximumWidth(80);
    m_cbIsOnset = new QCheckBox;
    m_cbIsOnset->setText(tr("Onset"));

    m_btnInsertAbove = new QPushButton;
    m_btnInsertAbove->setText(tr("Insert"));
    m_btnInsertAbove->setFixedSize(60, 24);

    m_btnDelete = new QPushButton;
    m_btnDelete->setText(tr("Delete"));
    m_btnDelete->setFixedSize(60, 24);

    connect(m_btnInsertAbove, &QPushButton::clicked, this, &PhonemeNameItemView::insertAboveClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &PhonemeNameItemView::deleteClicked);

    auto layout = new QHBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(8);
    layout->addWidget(m_cbLanguage);
    layout->addWidget(m_leName);
    layout->addWidget(m_cbIsOnset);
    layout->addStretch();
    layout->addWidget(m_btnInsertAbove);
    layout->addWidget(m_btnDelete);
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
