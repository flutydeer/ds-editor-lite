#ifndef DSCONNECTOR_STDIOTRANSPORT_H
#define DSCONNECTOR_STDIOTRANSPORT_H

#include <QByteArray>
#include <QFile>
#include <QObject>

class QThread;

namespace DsConnector {

    class StdioTransport final : public QObject {
        Q_OBJECT

    public:
        explicit StdioTransport(QObject *parent = nullptr);
        ~StdioTransport() override;

        bool start(QString *error = nullptr);

    public slots:
        void writeLine(const QByteArray &line);

    signals:
        void lineReceived(const QByteArray &line);
        void inputClosed();
        void transportError(const QString &error);

    private:
        QThread *m_reader = nullptr;
        QFile m_output;
    };

}

#endif // DSCONNECTOR_STDIOTRANSPORT_H
