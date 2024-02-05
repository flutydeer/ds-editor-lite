#include "AudioExportDialog.h"

#include <QtWidgets>

AudioExportDialog::AudioExportDialog(QWidget *parent) : QDialog(parent) {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowTitle(tr("Export Audio"));
    auto mainLayout = new QVBoxLayout;

    auto presetLayout = new QFormLayout;
    auto presetOptionLayout = new QHBoxLayout;
    auto presetComboBox = new QComboBox;
    presetOptionLayout->addWidget(presetComboBox, 1);
    auto presetSaveAsButton = new QPushButton(tr("Save as"));
    presetOptionLayout->addWidget(presetSaveAsButton);
    auto presetDeleteButton = new QPushButton(tr("Delete"));
    presetOptionLayout->addWidget(presetDeleteButton);
    presetLayout->addRow(tr("Preset"), presetOptionLayout);
    mainLayout->addLayout(presetLayout);

    auto mainOptionsLayout = new QHBoxLayout;

    auto leftLayout = new QVBoxLayout;

    auto pathGroupBox = new QGroupBox(tr("File Path"));
    auto pathLayout = new QFormLayout;
    auto fileDirectoryLayout = new QHBoxLayout;
    auto fileDirectoryEdit = new QLineEdit;
    fileDirectoryLayout->addWidget(fileDirectoryEdit, 1);
    auto fileDirectoryBrowseButton = new QPushButton(tr("Browse"));
    fileDirectoryLayout->addWidget(fileDirectoryBrowseButton);
    pathLayout->addRow(tr("Directory"), fileDirectoryLayout);
    auto fileNameLayout = new QHBoxLayout;
    auto fileNameEdit = new QLineEdit;
    fileNameLayout->addWidget(fileNameEdit, 1);
    auto fileNameTemplateButton = new QPushButton(tr("Template"));
    auto fileNameTemplateMenu = new QMenu(this);
    fileNameTemplateButton->setMenu(fileNameTemplateMenu);
    fileNameLayout->addWidget(fileNameTemplateButton);
    pathLayout->addRow(tr("Name"), fileNameLayout);
    pathGroupBox->setLayout(pathLayout);
    leftLayout->addWidget(pathGroupBox);

    auto previewGroupBox = new QGroupBox(tr("Files To Be Output"));
    auto previewLayout = new QVBoxLayout;
    auto previewFrame = new QTextEdit;
    previewFrame->setReadOnly(true);
    previewLayout->addWidget(previewFrame);
    previewGroupBox->setLayout(previewLayout);
    leftLayout->addWidget(previewGroupBox);

    mainOptionsLayout->addLayout(leftLayout);

    auto rightLayout = new QVBoxLayout;

    auto formatGroupBox = new QGroupBox(tr("Format"));
    auto formatLayout = new QFormLayout;
    auto formatTypeComboBox = new QComboBox;
    formatLayout->addRow(tr("Type"), formatTypeComboBox);
    auto formatOptionComboBox = new QComboBox;
    formatLayout->addRow(tr("Option"), formatOptionComboBox);
    auto vbrSlider = new QSlider(Qt::Horizontal);
    formatLayout->addRow(tr("VBR Quality"), vbrSlider);
    auto formatSampleRateSpinBox = new QDoubleSpinBox;
    formatSampleRateSpinBox->setRange(0.0, std::numeric_limits<double>::max());
    formatLayout->addRow(tr("Sample Rate"), formatSampleRateSpinBox);
    auto extensionNameComboBox = new QComboBox;
    extensionNameComboBox->setEditable(true);
    formatLayout->addRow(tr("Extension Name"), extensionNameComboBox);
    formatGroupBox->setLayout(formatLayout);
    rightLayout->addWidget(formatGroupBox);

    auto mixingGroupBox = new QGroupBox(tr("Mixer"));
    auto mixingLayout = new QFormLayout;
    auto sourceComboBox = new QComboBox;
    sourceComboBox->addItems({
        tr("All tracks"),
        tr("Selected tracks"),
        tr("Custom"),
    });
    mixingLayout->addRow(tr("Source"), sourceComboBox);
    auto sourceListWidget = new QListWidget;
    mixingLayout->addRow(sourceListWidget);
    auto mixingOptionComboBox = new QComboBox;
    mixingOptionComboBox->addItems({
        tr("Mixed"),
        tr("Seperated"),
        tr("Seperated (through master track)"),
    });
    mixingLayout->addRow(tr("Mixing Option"), mixingOptionComboBox);
    auto trackAffixLayout = new QHBoxLayout;
    auto trackAffixEdit = new QLineEdit;
    trackAffixLayout->addWidget(trackAffixEdit, 1);
    auto trackAffixTemplateButton = new QPushButton(tr("Template"));
    auto trackAffixTemplateMenu = new QMenu(this);
    trackAffixTemplateButton->setMenu(trackAffixTemplateMenu);
    trackAffixLayout->addWidget(trackAffixTemplateButton);
    mixingLayout->addRow(tr("Affix"), trackAffixLayout);
    auto enableMuteSoloCheckBox = new QCheckBox(tr("Enable mute/solo"));
    enableMuteSoloCheckBox->setChecked(true);
    mixingLayout->addRow(enableMuteSoloCheckBox);
    mixingGroupBox->setLayout(mixingLayout);
    rightLayout->addWidget(mixingGroupBox);

    auto timeRangeGroupBox = new QGroupBox(tr("Time Range"));
    auto timeRangeLayout = new QVBoxLayout;
    auto rangeOptionLayout = new QHBoxLayout;
    auto rangeSelectAllRadio = new QRadioButton(tr("All"));
    rangeSelectAllRadio->setChecked(true);
    rangeOptionLayout->addWidget(rangeSelectAllRadio);
    auto rangeLoopIntervalRadio = new QRadioButton(tr("Loop interval"));
    rangeOptionLayout->addWidget(rangeLoopIntervalRadio);
    rangeOptionLayout->addStretch();
    timeRangeLayout->addLayout(rangeOptionLayout);
    auto rangeSelectionLayout = new QHBoxLayout;
    auto rangeCustomRadio = new QRadioButton(tr("Custom"));
    rangeSelectionLayout->addWidget(rangeCustomRadio);
    auto rangeStartEdit = new QLineEdit; // TODO use a special widget;
    rangeSelectionLayout->addWidget(rangeStartEdit);
    rangeSelectionLayout->addWidget(new QLabel("~"));
    auto rangeEndEdit = new QLineEdit;
    rangeSelectionLayout->addWidget(rangeEndEdit);
    timeRangeLayout->addLayout(rangeSelectionLayout);
    timeRangeGroupBox->setLayout(timeRangeLayout);
    rightLayout->addWidget(timeRangeGroupBox);

    mainOptionsLayout->addLayout(rightLayout);

    mainLayout->addLayout(mainOptionsLayout);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto exportButton = new QPushButton(tr("Export"));
    exportButton->setDefault(true);
    buttonLayout->addWidget(exportButton);
    auto cancelButton = new QPushButton(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

}

AudioExportDialog::~AudioExportDialog() {
}
