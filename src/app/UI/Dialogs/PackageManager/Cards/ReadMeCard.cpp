//
// Created by FlutyDeer on 2025/10/2.
//

#include "ReadMeCard.h"

#include "UI/Controls/CardView.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>

ReadMeCard::ReadMeCard(QWidget *parent) {
    setAttribute(Qt::WA_StyledBackground);
    
    lbReadMe = new QLabel;
    lbReadMe->setObjectName("lbReadMe");
    lbReadMe->setWordWrap(true);

    auto layout = new QHBoxLayout;
    layout->addWidget(lbReadMe);
    layout->setContentsMargins({16, 16, 16, 16});
    layout->setSpacing(0);
    
    card()->setLayout(layout);
    
    setTitle(tr("ReadMe"));
}

void ReadMeCard::onDataContextChanged(const QString &dataContext) {
    if (dataContext.isEmpty()) {
        lbReadMe->setText(tr("No readme file."));
        return;
    }

    QFile file(dataContext);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        lbReadMe->setText(content);
        file.close();
    } else {
        lbReadMe->setText(tr("Failed to open file."));
    }
}
