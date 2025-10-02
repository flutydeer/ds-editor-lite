//
// Created by FlutyDeer on 2025/10/2.
//

#include "ReadMeCard.h"

#include "UI/Controls/CardView.h"

#include <QFile>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <qtconcurrentrun.h>
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

    lbReadMe->setText(tr("Loading..."));

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [=, this]() {
        lbReadMe->setText(watcher->result());
        watcher->deleteLater();
    });

    QFuture<QString> future = QtConcurrent::run([dataContext]() {
        QFile file(dataContext);
        // Check if file size exceeds 64KiB
        if (file.size() > 64 * 1024) {
            return QObject::tr("File is too large to read.");
        }
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();
            return content;
        } else {
            return QObject::tr("Failed to open file.");
        }
    });

    watcher->setFuture(future);
}
