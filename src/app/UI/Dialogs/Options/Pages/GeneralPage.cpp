//
// Created by fluty on 24-3-18.
//

#include "GeneralPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QLabel>
#include <QVBoxLayout>

GeneralPage::GeneralPage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->general();

    auto langKey = languageKeyFromType(option->defaultSingingLanguage);
    m_cbDefaultSingingLanguage = new LanguageComboBox(langKey);
    connect(m_cbDefaultSingingLanguage, &ComboBox::currentIndexChanged, this,
            &GeneralPage::modifyOption);

    auto defaultSingingLanguageItem = new OptionsCardItem;
    defaultSingingLanguageItem->setTitle(tr("Default Singing Language"));
    defaultSingingLanguageItem->addWidget(m_cbDefaultSingingLanguage);

    m_leDefaultLyric = new LineEdit;
    m_leDefaultLyric->setFixedWidth(80);
    m_leDefaultLyric->setText(option->defaultLyric);
    connect(m_leDefaultLyric, &LineEdit::editingFinished, this, &GeneralPage::modifyOption);
    // m_leDefaultLyric->setPlaceholderText(option->defaultLyric);

    auto defaultLyricItem = new OptionsCardItem;
    defaultLyricItem->setTitle(tr("Default Lyric"));
    defaultLyricItem->addWidget(m_leDefaultLyric);

    auto singingCardLayout = new QVBoxLayout;
    singingCardLayout->addWidget(defaultSingingLanguageItem);
    singingCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    singingCardLayout->addWidget(defaultLyricItem);
    singingCardLayout->setContentsMargins(10, 5, 10, 5);
    singingCardLayout->setSpacing(0);

    auto singingCard = new OptionsCard;
    singingCard->setTitle(tr("Singing"));
    singingCard->card()->setLayout(singingCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(singingCard);
    mainLayout->addSpacerItem(
        new QSpacerItem(8, 4, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}
void GeneralPage::modifyOption() {
    auto option = appOptions->general();
    option->defaultSingingLanguage =
        static_cast<AppGlobal::LanguageType>(m_cbDefaultSingingLanguage->currentIndex());
    option->defaultLyric = m_leDefaultLyric->text();
    appOptions->saveAndNotify();
}