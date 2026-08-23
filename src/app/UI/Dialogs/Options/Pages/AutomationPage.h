#ifndef AUTOMATIONPAGE_H
#define AUTOMATIONPAGE_H

#include "Bootstrap/StartupArguments.h"
#include "IOptionPage.h"

#include <QMap>
#include <QStringList>

class ComboBox;
class PathEditor;
class SwitchButton;
namespace SVS {
    class ExpressionSpinBox;
}

class AutomationPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AutomationPage(QWidget *parent = nullptr);

    // The caller supplies IDs from the currently filtered Public Automation Manifest.
    void setCustomPermissionOperationIds(QStringList operationIds);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    [[nodiscard]] QString sourceDescription(StartupArguments::ConfigSource source,
                                            const QString &optionName) const;

    SwitchButton *m_mcpEnabled = nullptr;
    SVS::ExpressionSpinBox *m_controlPort = nullptr;
    ComboBox *m_profile = nullptr;
    PathEditor *m_readRoots = nullptr;
    PathEditor *m_writeRoots = nullptr;
    QMap<QString, SwitchButton *> m_customPermissionSwitches;
    QStringList m_customPermissionOperationIds;
    StartupArguments::EffectiveAutomationConfig m_effectiveConfig;
};

#endif // AUTOMATIONPAGE_H
