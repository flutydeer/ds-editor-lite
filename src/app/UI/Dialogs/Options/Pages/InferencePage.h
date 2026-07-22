#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"
#include "Modules/Inference/Models/GpuInfo.h"

#include <QFutureWatcher>

class LineEdit;
class ComboBox;
class SwitchButton;
class SeekBarSpinboxGroup;
class DoubleSeekBarSpinboxGroup;
class OptionListCard;
class OptionsCardItem;
class QTreeView;

class InferencePage : public IOptionPage {
    Q_OBJECT

public:
    explicit InferencePage(QWidget *parent = nullptr);
    // ~InferencePage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    void requestGpuDetection();
    void startGpuDetection(const QString &provider);
    void showGpuDetectionPending();
    void applyGpuList(const QList<GpuInfo> &deviceList);

    ComboBox *m_cbExecutionProvider;
    ComboBox *m_cbDeviceList;
    OptionListCard *m_deviceCard;
    OptionsCardItem *m_gpuItem;
    QFutureWatcher<QList<GpuInfo>> *m_gpuDetectionWatcher;
    QString m_requestedGpuProvider;
    QString m_activeGpuProvider;
    ComboBox *m_cbSamplingSteps;
    DoubleSeekBarSpinboxGroup *m_dsDepthSlider;
    SwitchButton *m_swRunVocoderOnCpu;
    SwitchButton *m_autoStartInfer;
    SeekBarSpinboxGroup *m_smoothSlider;
    QTreeView *m_treeView;
};


#endif // INFERENCEPAGE_H
