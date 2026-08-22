#include "DsspPage.h"

#include "Model/AppOptions/AppOptions.h"
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/SwitchButton.h>

#include <QSpinBox>
#include <QVBoxLayout>

DsspPage::DsspPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void DsspPage::modifyOption() {
    const auto option = appOptions->dssp();
    option->enabled = m_swEnabled->value();
    option->host = m_leHost->text().trimmed();
    option->port = m_spPort->value();
    appOptions->saveAndNotify(AppOptionsGlobal::Dssp);
}

QWidget *DsspPage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->dssp();

    m_swEnabled = new SwitchButton(option->enabled);
    connect(m_swEnabled, &SwitchButton::toggled, this, &DsspPage::modifyOption);

    m_leHost = new LineEdit(option->host);
    m_leHost->setPlaceholderText(QStringLiteral("127.0.0.1"));
    connect(m_leHost, &LineEdit::editingFinished, this, &DsspPage::modifyOption);

    m_spPort = new QSpinBox;
    m_spPort->setRange(1, 65535);
    m_spPort->setValue(option->port);
    connect(m_spPort, &QSpinBox::valueChanged, this, [this] { modifyOption(); });

    const auto serviceCard = new OptionListCard(tr("DSSP Service"));
    serviceCard->addItem(tr("Enable service"),
                         tr("Serve synthesis, singer metadata and extraction over the DSSP "
                            "HTTP API. Changing the address restarts the service."),
                         m_swEnabled);
    serviceCard->addItem(tr("Listen address"), tr("IP address to bind"), m_leHost);
    serviceCard->addItem(tr("Port"), tr("TCP port to bind"), m_spPort);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(serviceCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
