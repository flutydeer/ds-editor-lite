#include "AudioExportDialog.h"

#include "Model/AppModel.h"
#include "Model/Track.h"
#include "Controls/Menu.h"

#include <QtWidgets>
#include <QSet>

AudioExportDialog::AudioExportDialog(QWidget *parent) : Dialog(parent), m_exporter(new AudioExporter(this)) {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowTitle(tr("Export Audio"));
    auto mainLayout = new QVBoxLayout;

    auto presetLayout = new QFormLayout;
    auto presetOptionLayout = new QHBoxLayout;
    m_presetComboBox = new QComboBox;
    presetOptionLayout->addWidget(m_presetComboBox, 1);
    auto presetSaveAsButton = new Button(tr("Save as"));
    presetOptionLayout->addWidget(presetSaveAsButton);
    m_presetDeleteButton = new Button(tr("Delete"));
    presetOptionLayout->addWidget(m_presetDeleteButton);
    presetLayout->addRow(tr("Preset"), presetOptionLayout);
    mainLayout->addLayout(presetLayout);

    auto mainOptionsLayout = new QHBoxLayout;

    auto leftLayout = new QVBoxLayout;

    auto pathGroupBox = new QGroupBox(tr("File Path"));
    auto pathLayout = new QFormLayout;
    auto fileDirectoryLayout = new QHBoxLayout;
    m_fileDirectoryEdit = new QLineEdit;
    m_fileDirectoryEdit->setPlaceholderText(tr("(Project directory)"));
    fileDirectoryLayout->addWidget(m_fileDirectoryEdit, 1);
    auto fileDirectoryBrowseButton = new Button(tr("Browse"));
    fileDirectoryLayout->addWidget(fileDirectoryBrowseButton);
    pathLayout->addRow(tr("Directory"), fileDirectoryLayout);
    auto fileNameLayout = new QHBoxLayout;
    m_fileNameEdit = new QLineEdit;
    fileNameLayout->addWidget(m_fileNameEdit, 1);
    auto fileNameTemplateButton = new Button(tr("Template"));
    auto fileNameTemplateMenu = new Menu(this);
    fileNameTemplateButton->setMenu(fileNameTemplateMenu);
    fileNameLayout->addWidget(fileNameTemplateButton);
    pathLayout->addRow(tr("Name"), fileNameLayout);
    pathGroupBox->setLayout(pathLayout);
    leftLayout->addWidget(pathGroupBox);

    auto previewGroupBox = new QGroupBox(tr("Files To Be Exported"));
    auto previewLayout = new QVBoxLayout;
    m_previewFrame = new QTextEdit;
    m_previewFrame->setReadOnly(true);
    previewLayout->addWidget(m_previewFrame);
    previewGroupBox->setLayout(previewLayout);
    leftLayout->addWidget(previewGroupBox);

    mainOptionsLayout->addLayout(leftLayout);

    auto rightLayout = new QVBoxLayout;

    auto formatGroupBox = new QGroupBox(tr("Format"));
    auto formatLayout = new QFormLayout;
    m_formatTypeComboBox = new QComboBox;
    for (const auto &format : AudioExporter::formats()) {
        m_formatTypeComboBox->addItem(format.formatName, format.flag);
    }
    formatLayout->addRow(tr("Type"), m_formatTypeComboBox);
    m_formatOptionComboBox = new QComboBox;
    formatLayout->addRow(tr("Option"), m_formatOptionComboBox);
    m_vbrSlider = new QSlider(Qt::Horizontal);
    formatLayout->addRow(tr("Quality"), m_vbrSlider);
    m_formatSampleRateSpinBox = new QDoubleSpinBox;
    m_formatSampleRateSpinBox->setRange(0.0, std::numeric_limits<double>::max());
    formatLayout->addRow(tr("Sample Rate"), m_formatSampleRateSpinBox);
    m_extensionNameEdit = new QLineEdit;
    formatLayout->addRow(tr("Extension"), m_extensionNameEdit);
    formatGroupBox->setLayout(formatLayout);
    rightLayout->addWidget(formatGroupBox);

    auto mixingGroupBox = new QGroupBox(tr("Mixer"));
    auto mixingLayout = new QFormLayout;
    m_sourceComboBox = new QComboBox;
    m_sourceComboBox->addItems({
        tr("All tracks"),
        tr("Selected tracks"),
        tr("Custom"),
    });
    mixingLayout->addRow(tr("Source"), m_sourceComboBox);
    m_sourceListWidget = new QListWidget;
    for (int i = 0; i < AppModel::instance()->tracks().size(); i++) {
        auto track = AppModel::instance()->tracks()[i];
        auto item = new QListWidgetItem(QString("%1 - %2").arg(i + 1).arg(track->name()));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        m_sourceListWidget->addItem(item);
        item->setCheckState(Qt::Unchecked);
    }
    mixingLayout->addRow(m_sourceListWidget);
    m_mixingOptionComboBox = new QComboBox;
    m_mixingOptionComboBox->addItems({
        tr("Mixed"),
        tr("Seperated"),
        tr("Seperated (through master track)"),
    });
    mixingLayout->addRow(tr("Mixing Option"), m_mixingOptionComboBox);
    auto trackAffixLayout = new QHBoxLayout;
    m_trackAffixEdit = new QLineEdit;
    trackAffixLayout->addWidget(m_trackAffixEdit, 1);
    m_trackAffixTemplateButton = new Button(tr("Template"));
    auto trackAffixTemplateMenu = new Menu(this);
    m_trackAffixTemplateButton->setMenu(trackAffixTemplateMenu);
    trackAffixLayout->addWidget(m_trackAffixTemplateButton);
    mixingLayout->addRow(tr("Affix"), trackAffixLayout);
    m_enableMuteSoloCheckBox = new QCheckBox(tr("Enable mute/solo"));
    m_enableMuteSoloCheckBox->setChecked(true);
    mixingLayout->addRow(m_enableMuteSoloCheckBox);
    mixingGroupBox->setLayout(mixingLayout);
    rightLayout->addWidget(mixingGroupBox);

    auto timeRangeGroupBox = new QGroupBox(tr("Time Range"));
    auto timeRangeLayout = new QVBoxLayout;
    auto rangeOptionLayout = new QHBoxLayout;
    m_rangeSelectAllRadio = new QRadioButton(tr("All"));
    m_rangeSelectAllRadio->setChecked(true);
    rangeOptionLayout->addWidget(m_rangeSelectAllRadio);
    m_rangeLoopIntervalRadio = new QRadioButton(tr("Loop interval"));
    rangeOptionLayout->addWidget(m_rangeLoopIntervalRadio);
    rangeOptionLayout->addStretch();
    timeRangeLayout->addLayout(rangeOptionLayout);
    timeRangeGroupBox->setLayout(timeRangeLayout);
    rightLayout->addWidget(timeRangeGroupBox);

    mainOptionsLayout->addLayout(rightLayout);

    mainLayout->addLayout(mainOptionsLayout);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    m_warningButton = new Button;
    m_warningButton->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxWarning));
    m_warningButton->setToolTip(tr("Warning"));
    m_warningButton->setVisible(false);
    buttonLayout->addWidget(m_warningButton);
    auto exportButton = new Button(tr("Export"));
    exportButton->setPrimary(true);
    exportButton->setDefault(true);
    buttonLayout->addWidget(exportButton);
    auto cancelButton = new Button(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    connect(fileNameTemplateMenu->addAction(tr("Project Name")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${projectName}");
    });
    // ======== TODO these two below are specialized for DsEditorLite. Will be removed in DiffScope.
    connect(fileNameTemplateMenu->addAction(tr("Tempo")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${tempo}");
    });
    connect(fileNameTemplateMenu->addAction(tr("Time Signature")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${projectName}");
    });
    // ========
    connect(fileNameTemplateMenu->addAction(tr("Sample Rate")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${sampleRate}");
    });
    connect(fileNameTemplateMenu->addAction(tr("Current Date")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${today}");
    });
    connect(fileNameTemplateMenu->addAction(tr("Affix of Each Track")), &QAction::triggered, this, [=] {
        m_fileNameEdit->setText(m_fileNameEdit->text() + "${trackAffix}");
    });

    connect(trackAffixTemplateMenu->addAction(tr("Track Name")), &QAction::triggered, this, [=] {
        m_trackAffixEdit->setText(m_trackAffixEdit->text() + "${trackName}");
    });
    connect(trackAffixTemplateMenu->addAction(tr("Track Index")), &QAction::triggered, this, [=] {
        m_trackAffixEdit->setText(m_trackAffixEdit->text() + "${trackIndex}");
    });
    connect(trackAffixTemplateMenu->addAction(tr("Gain")), &QAction::triggered, this, [=] {
        m_trackAffixEdit->setText(m_trackAffixEdit->text() + "${gain}");
    });
    connect(trackAffixTemplateMenu->addAction(tr("Pan")), &QAction::triggered, this, [=] {
        m_trackAffixEdit->setText(m_trackAffixEdit->text() + "${pan}");
    });

    updatePresetList();

    connect(m_fileDirectoryEdit, &QLineEdit::textChanged, this, &AudioExportDialog::onFormModified);
    connect(m_fileNameEdit, &QLineEdit::textChanged, this, &AudioExportDialog::onFormModified);
    connect(m_formatTypeComboBox, &QComboBox::currentIndexChanged, this, [=] {
        m_extensionNameEdit->clear();
        onFormModified();
    });
    connect(m_formatOptionComboBox, &QComboBox::currentIndexChanged, this, &AudioExportDialog::onFormModified);
    connect(m_vbrSlider, &QSlider::valueChanged, this, &AudioExportDialog::onFormModified);
    connect(m_formatSampleRateSpinBox, &QDoubleSpinBox::valueChanged, this, &AudioExportDialog::onFormModified);
    connect(m_extensionNameEdit, &QLineEdit::textChanged, this, &AudioExportDialog::onFormModified);
    connect(m_sourceComboBox, &QComboBox::currentIndexChanged, this, &AudioExportDialog::onFormModified);
    connect(m_sourceListWidget, &QListWidget::itemChanged, this, &AudioExportDialog::onFormModified);
    connect(m_mixingOptionComboBox, &QComboBox::currentIndexChanged, this, &AudioExportDialog::onFormModified);
    connect(m_trackAffixEdit, &QLineEdit::textChanged, this, &AudioExportDialog::onFormModified);
    connect(m_enableMuteSoloCheckBox, &QCheckBox::stateChanged, this, &AudioExportDialog::onFormModified);
    connect(m_rangeSelectAllRadio, &QRadioButton::toggled, this, &AudioExportDialog::onFormModified);
    connect(m_warningButton, &QPushButton::clicked, this, [=] {
        QMessageBox::warning(this, {}, m_warningText);
    });
    connect(presetSaveAsButton, &QPushButton::clicked, this, &AudioExportDialog::presetSaveAs);
    connect(m_presetDeleteButton, &QPushButton::clicked, this, [=] {
        AudioExporter::deletePreset(m_presetComboBox->itemData(m_presetComboBox->currentIndex()).toString());
        updatePresetList();
    });
}

AudioExportDialog::~AudioExportDialog() {
}

void AudioExportDialog::applyExporterOptionToDialog() {
    m_holdFormModified = true;
    auto option = m_exporter->option();
    if (option.fileDirectory != m_fileDirectoryEdit->text()) {
        m_fileDirectoryEdit->setText(option.fileDirectory);
    }
    if (m_exporter->option().fileName != m_fileNameEdit->text())
        m_fileNameEdit->setText(m_exporter->option().fileName);
    auto formatIndex = AudioExporter::findFormatIndex(option.formatFlag);
    if (formatIndex == -1) {
        QMessageBox::critical(this, {}, tr("Invalid option values!"));
        return;
    }
    auto formatInfo = AudioExporter::formats()[formatIndex];
    m_formatTypeComboBox->setCurrentIndex(formatIndex);
    m_formatOptionComboBox->clear();
    if (formatInfo.options.isEmpty()) {
        m_formatOptionComboBox->setDisabled(true);
    } else {
        m_formatOptionComboBox->setDisabled(false);
        for (const auto &[name, flag] : formatInfo.options) {
            m_formatOptionComboBox->addItem(name, flag);
        }
        auto optionIndex = formatInfo.findOptionIndex(option.formatFlag);
        if (optionIndex == -1) {
            QMessageBox::critical(this, {}, tr("Invalid option values!"));
            return;
        }
        m_formatOptionComboBox->setCurrentIndex(optionIndex);
    }
    m_vbrSlider->setEnabled(formatInfo.isVBRAvailable);
    if (formatInfo.isVBRAvailable) {
        m_vbrSlider->setValue(option.vbrQuality);
    } else {
        m_vbrSlider->setValue(100);
    }
    if (!qFuzzyCompare(option.sampleRate, m_formatSampleRateSpinBox->value()))
        m_formatSampleRateSpinBox->setValue(option.sampleRate);
    m_extensionNameEdit->setText(option.extensionName);
    m_sourceComboBox->setCurrentIndex(option.sourceOption);
    switch (option.sourceOption) {
        case AudioExporter::Option::AllTracks:
            for (int i = 0; i < m_sourceListWidget->count(); i++) {
                m_sourceListWidget->item(i)->setCheckState(Qt::Checked);
            }
            m_sourceListWidget->setDisabled(true);
            break;
        case AudioExporter::Option::SelectedTracks:
            for (int i = 0; i < m_sourceListWidget->count(); i++) {
                m_sourceListWidget->item(i)->setCheckState(AppModel::instance()->selectedTrackIndex() == i ? Qt::Checked : Qt::Unchecked);
            }
            m_sourceListWidget->setDisabled(true);
            break;
        case AudioExporter::Option::CustomTracks:
            for (int i = 0; i < m_sourceListWidget->count(); i++) {
                m_sourceListWidget->item(i)->setCheckState(Qt::Unchecked);
            }
            for (auto index : option.selectedTrackIndices) {
                m_sourceListWidget->item(index)->setCheckState(Qt::Checked);
            }
            m_sourceListWidget->setDisabled(false);
            break;
    }
    m_mixingOptionComboBox->setCurrentIndex(option.mixingOption);
    m_trackAffixEdit->setDisabled(option.mixingOption == AudioExporter::Option::Mixed);
    m_trackAffixTemplateButton->setDisabled(option.mixingOption == AudioExporter::Option::Mixed);
    if (option.affix != m_trackAffixEdit->text())
        m_trackAffixEdit->setText(option.affix);
    switch (option.timeRangeOption) {
        case AudioExporter::Option::All:
            m_rangeSelectAllRadio->setChecked(true);
            break;
        case AudioExporter::Option::LoopInterval:
            m_rangeLoopIntervalRadio->setChecked(true);
            break;
        case AudioExporter::Option::CustomRange:
            // TODO
            break;
    }
    auto fileList = m_exporter->outputFileList();
    m_previewFrame->clear();
    m_previewFrame->insertPlainText(fileList.join('\n'));
    int warningFlags = checkFileListWarnings(fileList);
    m_warningText.clear();
    QStringList warningList;
    if (warningFlags & EmptyWarning) {
        warningList += tr("No file will be exported.");
    }
    if (warningFlags & DuplicatedWarning) {
        warningList += tr("The files to be exported contain duplicated items.");
    }
    if (warningFlags & OverwritingWarning) {
        warningList += tr("Existing files will be overwritten after exporting.");
    }
    m_warningText = warningList.join("\n \n");
    m_warningButton->setVisible(warningFlags);
    m_holdFormModified = false;
}

void AudioExportDialog::applyDialogToExporterOption() {
    AudioExporter::Option option;
    option.fileDirectory = m_fileDirectoryEdit->text();
    option.fileName = m_fileNameEdit->text();
    option.formatFlag = m_formatTypeComboBox->itemData(m_formatTypeComboBox->currentIndex()).toInt();
    auto formatInfo = AudioExporter::formats()[m_formatTypeComboBox->currentIndex()];
    if (m_formatOptionComboBox->isEnabled() && formatInfo.findOptionIndex(m_formatOptionComboBox->itemData(m_formatOptionComboBox->currentIndex()).toInt()) != -1)
        option.formatFlag |= m_formatOptionComboBox->itemData(m_formatOptionComboBox->currentIndex()).toInt();
    else if (!formatInfo.options.isEmpty())
        option.formatFlag |= formatInfo.options[0].second;
    option.vbrQuality = m_vbrSlider->value();
    option.sampleRate = m_formatSampleRateSpinBox->value();
    if (m_extensionNameEdit->text().isEmpty())
        option.extensionName = formatInfo.extensionName;
    else
        option.extensionName = m_extensionNameEdit->text();
    option.sourceOption = AudioExporter::Option::SourceOption(m_sourceComboBox->currentIndex());
    if (option.sourceOption == AudioExporter::Option::CustomTracks) {
        for (int i = 0; i < m_sourceListWidget->count(); i++) {
            if (m_sourceListWidget->item(i)->checkState() == Qt::Checked)
                option.selectedTrackIndices.append(i);
        }
    }
    option.mixingOption = AudioExporter::Option::MixingOption(m_mixingOptionComboBox->currentIndex());
    option.affix = m_trackAffixEdit->text();
    option.enableMuteSolo = m_enableMuteSoloCheckBox->isChecked();
    option.timeRangeOption = m_rangeSelectAllRadio->isChecked() ? AudioExporter::Option::All : AudioExporter::Option::LoopInterval;
    m_exporter->setOption(option);
    updateDirtyPreset();
}

int AudioExportDialog::checkFileListWarnings(const QStringList &list) {
    if (list.isEmpty())
        return EmptyWarning;
    int flag = 0;
    QSet<QString> fileListSet;
    for (const auto &str : list) {
        fileListSet.insert(str);
        if (fileListSet.size() != list.size()) {
            flag |= DuplicatedWarning;
            break;
        }
    }
    for (const auto &str : list) {
        if (QFileInfo(str).exists()) {
            flag |= OverwritingWarning;
            break;
        }
    }
    return flag;
}

void AudioExportDialog::onFormModified() {
    if (m_holdFormModified)
        return;
    applyDialogToExporterOption();
}

void AudioExportDialog::loadPreset(const QVariant &data) {
    if (data.typeId() == QMetaType::QString) {
        m_presetDeleteButton->setDisabled(false);
        if (!m_exporter->loadPreset(data.toString())) {
            QMessageBox::critical(this, {}, tr("Cannot load preset: %1!").arg(data.toString()));
        }
    } else {
        m_presetDeleteButton->setDisabled(true);
        QSettings settings;
        settings.setValue("audio/lastUsedExportPreset", data.toInt());
        m_exporter->setOption(m_exporter->builtInPresets()[data.toInt()].second);
    }
    applyExporterOptionToDialog();
}

void AudioExportDialog::updateDirtyPreset() {
    m_exporter->savePreset({});
    updatePresetList();
}

void AudioExportDialog::presetSaveAs() {
    bool ok;
    auto name = QInputDialog::getText(this, {}, tr("Preset Name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.isEmpty())
        return;
    m_exporter->savePreset(name);
    updatePresetList();

}

void AudioExportDialog::updatePresetList() {
    disconnect(m_presetComboBox, nullptr, this, nullptr);
    m_presetComboBox->clear();
    auto builtInPresets = AudioExporter::builtInPresets();
    for (int i = 0; i < builtInPresets.size(); i++) {
        m_presetComboBox->addItem(builtInPresets[i].first, i);
    }

    auto lastUsedPreset = AudioExporter::lastUsedPreset();

    if (lastUsedPreset.typeId() != QMetaType::QString)
        m_presetComboBox->setCurrentIndex(lastUsedPreset.toInt());

    for (const auto &presetName : AudioExporter::presets()) {
        m_presetComboBox->addItem(presetName, presetName);
        if (lastUsedPreset.typeId() == QMetaType::QString && presetName == lastUsedPreset)
            m_presetComboBox->setCurrentIndex(m_presetComboBox->count() - 1);
    }

    if (m_exporter->loadPreset({})) {
        m_presetComboBox->addItem(tr("* Unsaved preset *"), QString());
        if (lastUsedPreset.typeId() == QMetaType::QString && lastUsedPreset.toString().isEmpty())
            m_presetComboBox->setCurrentIndex(m_presetComboBox->count() - 1);
    }

    loadPreset(m_presetComboBox->itemData(m_presetComboBox->currentIndex()));
    connect(m_presetComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        loadPreset(m_presetComboBox->itemData(index));
    });
}
