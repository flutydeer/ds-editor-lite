#ifndef NATIVEJSONRPCDISPATCHER_H
#define NATIVEJSONRPCDISPATCHER_H

#include <QJsonObject>
#include <QJsonValue>

namespace Automation {

    class PublicAutomationRegistry;

    class NativeJsonRpcDispatcher final {
    public:
        explicit NativeJsonRpcDispatcher(PublicAutomationRegistry &registry);

        [[nodiscard]] QJsonObject dispatch(const QJsonValue &message,
                                           const QString &clientId) const;

        [[nodiscard]] static QJsonObject parseError();

    private:
        PublicAutomationRegistry &m_registry;
    };

} // namespace Automation

#endif // NATIVEJSONRPCDISPATCHER_H
