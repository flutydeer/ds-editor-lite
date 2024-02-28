//
// Created by fluty on 24-2-20.
//

#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QMessageBox>

class MessageDialog : public QMessageBox {
public:
    explicit MessageDialog(QWidget *parent = nullptr);
};



#endif //MESSAGEDIALOG_H
