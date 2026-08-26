#ifndef AUTOMATIONPAGE_H
#define AUTOMATIONPAGE_H

#include "Bootstrap/StartupArguments.h"
#include "IOptionPage.h"

#include <QMap>
#include <QStringList>

class ComboBox;
class Button;
class QEvent;
class OptionListCard;
class OptionsCardItem;
class PathEditor;
class QPlainTextEdit;
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
    [[nodiscard]] QString sourceDescription(StartupArguments::ConfigSource source,
                                            const QString &optionName) const;
    [[nodiscard]] QString categoryDisplayName(const QString &category) const;
    [[nodiscard]] QString runtimeStateDescription(const QString &state) const;
    void refreshCategoryPermissionSwitches();
    void refreshConnectionConfigurations();
    void refreshRuntimeStatus();

    SwitchButton *m_mcpEnabled = nullptr;
    Button *m_refreshControlPort = nullptr;
    SVS::ExpressionSpinBox *m_controlPort = nullptr;
    ComboBox *m_profile = nullptr;
    PathEditor *m_readRoots = nullptr;
    PathEditor *m_writeRoots = nullptr;
    QMap<QString, SwitchButton *> m_customPermissionSwitches;
    QMap<QString, SwitchButton *> m_customCategorySwitches;
    QMap<QString, OptionListCard *> m_customCategoryCards;
    QMap<QString, QStringList> m_customCategoryOperationIds;
    QStringList m_customPermissionOperationIds;
    OptionsCardItem *m_runtimeStateItem = nullptr;
    OptionsCardItem *m_runtimeEndpointItem = nullptr;
    OptionsCardItem *m_runtimeErrorItem = nullptr;
    OptionsCardItem *m_readRootsItem = nullptr;
    OptionsCardItem *m_writeRootsItem = nullptr;
    QPlainTextEdit *m_stdioConfiguration = nullptr;
    QPlainTextEdit *m_streamableHttpConfiguration = nullptr;
    StartupArguments::EffectiveAutomationConfig m_effectiveConfig;
};

#endif // AUTOMATIONPAGE_H
