#include "ReadMeCard.h"

#include <lite/GUI/Controls/CardView.h>

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QTextStream>

ReadMeCard::ReadMeCard(QWidget *parent) : OptionsCard(parent) {
    setAttribute(Qt::WA_StyledBackground);

    lbReadMe = new QLabel;
    lbReadMe->setObjectName("lbReadMe");
    lbReadMe->setWordWrap(true);
    lbReadMe->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

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
    // Check if file size exceeds 64KiB
    if (file.size() > 64 * 1024) {
        lbReadMe->setText(tr("File is too large to read."));
        return;
    }
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        lbReadMe->setText(in.readAll());
        file.close();
    } else {
        lbReadMe->setText(tr("Failed to open file."));
    }
}
