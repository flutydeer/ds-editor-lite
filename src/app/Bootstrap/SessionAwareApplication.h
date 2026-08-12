#ifndef SESSIONAWAREAPPLICATION_H
#define SESSIONAWAREAPPLICATION_H

#include <QApplication>

class SessionAwareApplication final : public QApplication {
public:
    SessionAwareApplication(int &argc, char **argv);

    bool notify(QObject *receiver, QEvent *event) override;
};

#endif // SESSIONAWAREAPPLICATION_H
