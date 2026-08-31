#ifndef AUTOMATIONPAGE_H
#define AUTOMATIONPAGE_H

#include "Bootstrap/StartupArguments.h"
#include "IOptionPage.h"

#include <QMap>
#include <QStringList>

class ComboBox;
class Button;
class QEvent;
class QLabel;
class OptionListCard;
class OptionsCardItem;
class PathEditor;
class SwitchButton;

namespace SVS {
    class ExpressionSpinBox;
}

class AutomationPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AutomationPage(QWidget *parent = nullptr);

    // The caller supplies IDs from the shared Public Automation contract registry.
    void setCustomPermissionOperationIds(QStringList operationIds);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QString settingDescription(const QString &description,
                                             StartupArguments::ConfigSource source) const;
    [[nodiscard]] QString categoryDisplayName(const QString &category) const;
    [[nodiscard]] QString runtimeStateDescription(const QString &state) const;
    [[nodiscard]] QString currentMcpEndpoint() const;
    void refreshCategoryPermissionSwitches();
    void refreshRuntimeStatus();

    SwitchButton *m_mcpEnabled = nullptr;
    Button *m_randomizeControlPort = nullptr;
    SVS::ExpressionSpinBox *m_controlPort = nullptr;
    ComboBox *m_controlLevel = nullptr;
    PathEditor *m_accessRoots = nullptr;
    QMap<QString, SwitchButton *> m_customPermissionSwitches;
    QMap<QString, SwitchButton *> m_customCategorySwitches;
    QMap<QString, OptionsCardItem *> m_customCategoryHeaderItems;
    QMap<QString, QStringList> m_customCategoryOperationIds;
    QStringList m_customPermissionOperationIds;
    OptionListCard *m_serverCard = nullptr;
    QLabel *m_runtimeStateValue = nullptr;
    QLabel *m_runtimeErrorValue = nullptr;
    OptionsCardItem *m_runtimeErrorItem = nullptr;
    OptionsCardItem *m_accessRootsItem = nullptr;
    StartupArguments::EffectiveAutomationConfig m_effectiveConfig;
};

#endif // AUTOMATIONPAGE_H
