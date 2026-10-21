#ifndef NATIVEJSONRPCDISPATCHER_H
#define NATIVEJSONRPCDISPATCHER_H

#include <QJsonObject>
#include <QJsonValue>

#include <functional>

namespace Automation {

    class PublicAutomationRegistry;

    class NativeJsonRpcDispatcher final {
    public:
        explicit NativeJsonRpcDispatcher(PublicAutomationRegistry &registry,
                                         std::function<QString()> endpoint = {});

        [[nodiscard]] QJsonObject dispatch(const QJsonValue &message,
                                           const QString &clientId) const;

        [[nodiscard]] static QJsonObject parseError();

    private:
        PublicAutomationRegistry &m_registry;
        std::function<QString()> m_endpoint;
    };

} // namespace Automation

#endif // NATIVEJSONRPCDISPATCHER_H
