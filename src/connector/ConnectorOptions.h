#ifndef DSCONNECTOR_CONNECTOROPTIONS_H
#define DSCONNECTOR_CONNECTOROPTIONS_H

#include <lite/AutomationWire/ExposurePolicy.h>

#include <QString>
#include <QStringList>

namespace DsConnector {

    struct ConnectorOptions {
        AutomationWire::ExposureConfig exposure;
        int upstreamTimeoutMs = 30000;
    };

    bool parseConnectorOptions(const QStringList &arguments, ConnectorOptions &options,
                               QString &error);

}

#endif // DSCONNECTOR_CONNECTOROPTIONS_H
