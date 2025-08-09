#include "AudioExportDialog.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Modules/Audio/AudioSettings.h"

#include <QApplication>
#include <limits>

#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QMenu>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QRadioButton>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressBar>
#include <QTimer>

#include "UI/Controls/SvsExpressionSpinBox.h"

#include <Modules/Audio/AudioExporter_p.h>

namespace Audio::Internal {

    static AudioExporterConfig m_temporaryPreset;
    static bool m_hasTemporaryPreset = false;

    AudioExportDialog::AudioExportDialog(Core::IProjectWindow *windowHandle, QWidget *parent)
        : QDialog(parent), m_audioExporter(new AudioExporter(windowHandle, this)) {
        setWindowTitle(tr("Export Audio"));
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        auto mainLayout = new QVBoxLayout;

        auto presetLayout = new QFormLayout;
        auto presetOptionLayout = new QHBoxLayout;
        m_presetComboBox = new QComboBox;
        for (const auto &[presetName, presetConfig] : AudioExporter::predefinedPresets()) {
            m_presetComboBox->addItem(presetName);
        }
        for (const auto &presetName : AudioExporter::presets()) {
            m_presetComboBox->addItem(presetName, presetName);
        }
        presetOptionLayout->addWidget(m_presetComboBox, 1);
        auto presetSaveAsButton = new QPushButton(tr("Save &As..."));
        presetOptionLayout->addWidget(presetSaveAsButton);
        m_presetDeleteButton = new QPushButton(tr("&Delete"));
        presetOptionLayout->addWidget(m_presetDeleteButton);
        auto presetLabel = new QLabel(tr("&Preset"));
        presetLabel->setBuddy(m_presetComboBox);
        presetLayout->addRow(presetLabel, presetOptionLayout);
        mainLayout->addLayout(presetLayout);

        auto mainOptionsLayout = new QVBoxLayout;

        auto leftLayout = new QVBoxLayout;
        auto rightLayout = new QVBoxLayout;

        auto pathGroupBox = new QGroupBox(tr("File Path"));
        auto pathLayout = new QFormLayout;
        auto filePathBrowseButton = new QPushButton(tr("&Browse..."));
        pathLayout->addRow(filePathBrowseButton);
        auto fileNameLayout = new QHBoxLayout;
        m_fileNameEdit = new QLineEdit;
        fileNameLayout->addWidget(m_fileNameEdit, 1);
        auto fileNameTemplateButton = new QPushButton;
        fileNameTemplateButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
        fileNameTemplateButton->setToolTip(tr("Template"));
        auto fileNameTemplateMenu = new QMenu(this);
        fileNameTemplateMenu->addAction(QStringLiteral("${projectName}"), this, [this] {
            const QFileInfo fileInfo(m_fileNameEdit->text());
            m_fileNameEdit->setText(fileInfo.completeBaseName() +
                                    QStringLiteral("${projectName}.") + fileInfo.suffix());
        });
        fileNameTemplateMenu->addAction(QStringLiteral("${sampleRate}"), this, [this] {
            const QFileInfo fileInfo(m_fileNameEdit->text());
            m_fileNameEdit->setText(fileInfo.completeBaseName() + QStringLiteral("${sampleRate}.") +
                                    fileInfo.suffix());
        });
        fileNameTemplateMenu->addAction(QStringLiteral("${today}"), this, [this] {
            const QFileInfo fileInfo(m_fileNameEdit->text());
            m_fileNameEdit->setText(fileInfo.completeBaseName() + QStringLiteral("${today}.") +
                                    fileInfo.suffix());
        });
        auto fileNameTemplateTrackNameAction =
            fileNameTemplateMenu->addAction(QStringLiteral("${trackName}"), this, [this] {
                const QFileInfo fileInfo(m_fileNameEdit->text());
                m_fileNameEdit->setText(fileInfo.completeBaseName() +
                                        QStringLiteral("${trackName}.") + fileInfo.suffix());
            });
        auto fileNameTemplateTrackIndexAction =
            fileNameTemplateMenu->addAction(QStringLiteral("${trackIndex}"), this, [this] {
                const QFileInfo fileInfo(m_fileNameEdit->text());
                m_fileNameEdit->setText(fileInfo.completeBaseName() +
                                        QStringLiteral("${trackIndex}.") + fileInfo.suffix());
            });
        fileNameTemplateButton->setMenu(fileNameTemplateMenu);
        fileNameLayout->addWidget(fileNameTemplateButton);
        auto fileNameTemplateContextHelpButton = new QPushButton;
        fileNameTemplateContextHelpButton->setToolTip(
            tr("<p>You can specify the name (including extension name) of exported files.</p>"
               "<p>Template tags in the file name will be replaced with the corresponding text. "
               "The following are the available template tags:</p>"
               "<ul>"
               "<li><b>${projectName}</b>: the base name of the project file (excluding extension "
               "name)</li>"
               "<li><b>${sampleRate}</b>: the sample rate specified in export configuration</li>"
               "<li><b>${today}</b>: today's date in \"yyyyMMdd\" format (e. g. \"19260817\" for "
               "August 17, 1926)</li>"
               "<li><b>${$}</b>: a single \"$\" character</li>"
               "</ul>"
               "<p>In particular, the following template tags are available only when the mixing "
               "option is not \"mixed\":</p>"
               "<ul>"
               "<li><b>${trackName}</b>: the name of track</li>"
               "<li><b>${trackIndex}</b>: the index of track (starting from 1)</li>"
               "</ul>"
               "<p>You can select the template tag in the pop-up menu. The selected template tag "
               "will be appended to the end of the file name.</p>"));
        fileNameTemplateContextHelpButton->setIcon(
            style()->standardIcon(QStyle::SP_DialogHelpButton));
        fileNameLayout->addWidget(fileNameTemplateContextHelpButton);
        auto nameLabel = new QLabel(tr("&Name"));
        nameLabel->setBuddy(m_fileNameEdit);
        pathLayout->addRow(nameLabel, fileNameLayout);
        m_fileNamePreviewLabel = new QLabel;
        pathLayout->addWidget(m_fileNamePreviewLabel);
        m_fileDirectoryEdit = new QLineEdit;
        m_fileDirectoryEdit->setPlaceholderText(tr("(Project directory)"));
        pathLayout->addRow(tr("Dire&ctory"), m_fileDirectoryEdit);
        m_fileTypeComboBox = new QComboBox;
        m_fileTypeComboBox->addItems({tr("WAV"), tr("FLAC"), tr("Ogg Vorbis"), tr("MP3")});
        pathLayout->addRow(tr("&Type"), m_fileTypeComboBox);
        pathGroupBox->setLayout(pathLayout);
        leftLayout->addWidget(pathGroupBox);

        auto formatGroupBox = new QGroupBox(tr("Format"));
        auto formatLayout = new QFormLayout;

        m_formatMonoComboBox = new QComboBox;
        m_formatMonoComboBox->addItems({tr("Mono"), tr("Stereo")});
        formatLayout->addRow(tr("C&hannel"), m_formatMonoComboBox);
        m_formatOptionComboBox = new QComboBox;
        formatLayout->addRow(tr("&Option"), m_formatOptionComboBox);
        auto vbrLayout = new QHBoxLayout;
        m_vbrSlider = new QSlider(Qt::Horizontal);
        m_vbrSlider->setRange(0, 100);
        vbrLayout->addWidget(m_vbrSlider);
        auto vbrSpinBox = new SVS::ExpressionSpinBox;
        vbrSpinBox->setRange(0, 100);
        vbrLayout->addWidget(vbrSpinBox);
        connect(vbrSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_vbrSlider,
                &QSlider::valueChanged);
        connect(m_vbrSlider, &QSlider::valueChanged, vbrSpinBox, &QSpinBox::setValue);
        auto vbrLabel = new QLabel(tr("&Quality"));
        vbrLabel->setBuddy(vbrSpinBox);
        formatLayout->addRow(vbrLabel, vbrLayout);
        m_formatSampleRateComboBox = new QComboBox;
        m_formatSampleRateComboBox->setEditable(true);
        m_formatSampleRateComboBox->setValidator(
            new QDoubleValidator(0.01, std::numeric_limits<double>::max(), 2));
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        m_formatSampleRateComboBox->addItems(
            {locale.toString(8000), locale.toString(11025), locale.toString(12000),
             locale.toString(16000), locale.toString(22050), locale.toString(24000),
             locale.toString(32000), locale.toString(44100), locale.toString(48000),
             locale.toString(64000), locale.toString(88200), locale.toString(96000),
             locale.toString(128000), locale.toString(176400), locale.toString(192000),
             locale.toString(256000), locale.toString(352800), locale.toString(384000)});
        formatLayout->addRow(tr("&Sample rate"), m_formatSampleRateComboBox);
        formatGroupBox->setLayout(formatLayout);
        leftLayout->addWidget(formatGroupBox);

        auto mixingGroupBox = new QGroupBox(tr("Mixer"));
        auto mixingLayout = new QFormLayout;
        m_mixingOptionComboBox = new QComboBox;
        m_mixingOptionComboBox->addItems({
            tr("Mixed"),
            tr("Separated"),
            tr("Separated (through master track)"),
        });
        connect(m_mixingOptionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                fileNameTemplateTrackNameAction, &QAction::setEnabled);
        connect(m_mixingOptionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                fileNameTemplateTrackIndexAction, &QAction::setEnabled);
        mixingLayout->addRow(tr("&Mixing option"), m_mixingOptionComboBox);
        m_enableMuteSoloCheckBox = new QCheckBox(tr("Enable m&ute/solo"));
        m_enableMuteSoloCheckBox->setChecked(true);
        mixingLayout->addRow(m_enableMuteSoloCheckBox);
        m_sourceComboBox = new QComboBox;
        m_sourceComboBox->addItems({
            tr("All tracks"),
            tr("Selected tracks"),
            tr("Custom"),
        });
        mixingLayout->addRow(tr("&Source"), m_sourceComboBox);
        m_sourceListWidget = new QListWidget;
        m_sourceListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (const auto &trackName : projectTrackList()) {
            auto item = new QListWidgetItem(trackName);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            m_sourceListWidget->addItem(item);
        }
        connect(m_sourceListWidget, &QListWidget::itemChanged, this, [](QListWidgetItem *item) {
            item->setSelected(item->checkState() == Qt::Checked);
        });
        connect(m_sourceListWidget, &QListWidget::itemSelectionChanged, this, [this] {
            for (int i = 0; i < m_sourceListWidget->count(); i++) {
                const auto item = m_sourceListWidget->item(i);
                item->setCheckState(item->isSelected() ? Qt::Checked : Qt::Unchecked);
            }
        });

        mixingLayout->addRow(m_sourceListWidget);
        mixingGroupBox->setLayout(mixingLayout);
        rightLayout->addWidget(mixingGroupBox);

        auto timeRangeGroupBox = new QGroupBox(tr("Time Range"));
        auto timeRangeLayout = new QVBoxLayout;
        auto rangeOptionLayout = new QHBoxLayout;
        m_rangeSelectAllRadio = new QRadioButton(tr("A&ll"));
        m_rangeSelectAllRadio->setChecked(true);
        rangeOptionLayout->addWidget(m_rangeSelectAllRadio);
        m_rangeLoopIntervalRadio = new QRadioButton(tr("Loop s&ection"));
        rangeOptionLayout->addWidget(m_rangeLoopIntervalRadio);
        rangeOptionLayout->addStretch();
        timeRangeLayout->addLayout(rangeOptionLayout);
        timeRangeGroupBox->setLayout(timeRangeLayout);
        leftLayout->addWidget(timeRangeGroupBox);

        mainOptionsLayout->addLayout(leftLayout);
        mainOptionsLayout->addLayout(rightLayout);

        mainLayout->addLayout(mainOptionsLayout);

        m_keepOpenCheckBox = new QCheckBox(tr("&Keep this dialog open after successful export"));
        mainLayout->addWidget(m_keepOpenCheckBox);

        auto buttonLayout = new QHBoxLayout;
        auto dryRunButton = new QPushButton(tr("Dry &Run"));
        buttonLayout->addWidget(dryRunButton);
        auto dryRunContextHelpButton = new QPushButton;
        dryRunContextHelpButton->setIcon(style()->standardIcon(QStyle::SP_DialogHelpButton));
        dryRunContextHelpButton->setToolTip(tr(
            "\"Dry Run\" shows the paths of files to export. No files will actually be exported."));
        buttonLayout->addWidget(dryRunContextHelpButton);
        buttonLayout->addStretch();
        m_warningButton = new QPushButton;
        m_warningButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
        m_warningButton->setToolTip(tr("Warning"));
        buttonLayout->addWidget(m_warningButton);
        auto exportButton = new QPushButton(tr("Export"));
        // exportButton->setPrimary(true);
        exportButton->setDefault(true);
        buttonLayout->addWidget(exportButton);
        auto cancelButton = new QPushButton(tr("Cancel"));
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);

        setLayout(mainLayout);

        resize(600, 800);

        connect(m_fileNameEdit, &QLineEdit::textChanged, this, &AudioExportDialog::updateConfig);
        connect(m_fileDirectoryEdit, &QLineEdit::textChanged, this,
                &AudioExportDialog::updateConfig);
        connect(m_fileTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [vbrSpinBox, this](int index) {
                    const QFileInfo fileInfo(m_fileNameEdit->text());
                    m_fileNameEdit->setText(fileInfo.completeBaseName() + "." +
                                            AudioExporterConfig::extensionOfType(
                                                static_cast<AudioExporterConfig::FileType>(index)));
                    m_formatOptionComboBox->clear();
                    const auto options = AudioExporterConfig::formatOptionsOfType(
                        static_cast<AudioExporterConfig::FileType>(index));
                    if (options.isEmpty()) {
                        m_formatOptionComboBox->setDisabled(true);
                    } else {
                        m_formatOptionComboBox->setDisabled(false);
                        m_formatOptionComboBox->addItems(options);
                        m_formatOptionComboBox->setCurrentIndex(0);
                    }
                    if (index == AudioExporterConfig::FT_Wav) {
                        m_vbrSlider->setValue(100);
                        m_vbrSlider->setDisabled(true);
                        vbrSpinBox->setDisabled(true);
                    } else {
                        m_vbrSlider->setDisabled(false);
                        vbrSpinBox->setDisabled(false);
                    }
                    updateConfig();
                });
        connect(m_formatMonoComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &AudioExportDialog::updateConfig);
        connect(m_formatOptionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &AudioExportDialog::updateConfig);
        connect(m_vbrSlider, &QSlider::valueChanged, this, &AudioExportDialog::updateConfig);
        connect(m_formatSampleRateComboBox, &QComboBox::currentTextChanged, this,
                &AudioExportDialog::updateConfig);
        connect(m_sourceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](const int index) {
                    if (index == AudioExporterConfig::SO_All) {
                        m_sourceListWidget->setDisabled(false);
                        m_sourceListWidget->selectAll();
                        m_sourceListWidget->setDisabled(true);
                    } else if (index == AudioExporterConfig::SO_Selected) {
                        m_sourceListWidget->setDisabled(false);
                        m_sourceListWidget->clearSelection();
                        for (const auto i : m_audioExporter->d_func()->selectedSources()) {
                            m_sourceListWidget->item(i)->setSelected(true);
                        }
                        m_sourceListWidget->setDisabled(true);
                    } else {
                        m_sourceListWidget->setDisabled(false);
                    }
                    updateConfig();
                });
        connect(m_sourceListWidget, &QListWidget::itemSelectionChanged, this,
                &AudioExportDialog::updateConfig);
        connect(m_mixingOptionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](const int index) {
                    if (index == AudioExporterConfig::MO_Mixed) {
                        const QFileInfo fileInfo(m_fileNameEdit->text());
                        auto basename = fileInfo.completeBaseName();
                        auto suffix = fileInfo.suffix();
                        if (basename.endsWith("_${trackIndex}_${trackName}")) {
                            basename = basename.chopped(27);
                        }
                        if (suffix.isEmpty()) {
                            suffix = AudioExporterConfig::extensionOfType(
                                m_audioExporter->config().fileType());
                        }
                        m_fileNameEdit->setText(basename + "." + suffix);
                    } else {
                        const QFileInfo fileInfo(m_fileNameEdit->text());
                        auto basename = fileInfo.completeBaseName();
                        auto suffix = fileInfo.suffix();
                        if (!basename.contains("${trackIndex}") &&
                            !basename.contains("${trackName}")) {
                            basename += "_${trackIndex}_${trackName}";
                        }
                        if (suffix.isEmpty()) {
                            suffix = AudioExporterConfig::extensionOfType(
                                m_audioExporter->config().fileType());
                        }
                        m_fileNameEdit->setText(basename + "." + suffix);
                    }
                    updateConfig();
                });
        connect(m_enableMuteSoloCheckBox, &QAbstractButton::toggled, this,
                &AudioExportDialog::updateConfig);
        connect(m_rangeSelectAllRadio, &QAbstractButton::toggled, this,
                &AudioExportDialog::updateConfig);

        connect(filePathBrowseButton, &QAbstractButton::clicked, this,
                &AudioExportDialog::browseFile);
        connect(m_warningButton, &QAbstractButton::clicked, this, &AudioExportDialog::showWarnings);
        connect(dryRunButton, &QAbstractButton::clicked, this,
                &AudioExportDialog::showDryRunResult);

        connect(m_presetComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](const int index) {
                    const auto data = m_presetComboBox->itemData(index);
                    const auto config = data.isNull()
                                            ? AudioExporter::predefinedPresets().at(index).second
                                            : AudioExporter::preset(data.toString());
                    m_presetDeleteButton->setDisabled(data.isNull());
                    m_audioExporter->setConfig(config);
                    updateView();
                });
        connect(presetSaveAsButton, &QAbstractButton::clicked, this, [this] {
            QString result = m_presetComboBox->currentText();
            while (true) {
                result = QInputDialog::getText(this, {}, tr("Preset name"), {}, result);
                if (result.isEmpty())
                    return;
                if (AudioExporter::presets().contains(result)) {
                    if (QMessageBox::question(
                            this, {},
                            tr("Preset \"%1\" already exists. Overwrite it?").arg(result)) ==
                        QMessageBox::Yes)
                        break;
                } else {
                    break;
                }
            }
            const auto config = m_audioExporter->config();
            AudioExporter::addPreset(result, config);
            {
                for (int i = 0; i < m_presetComboBox->count(); i++) {
                    if (m_presetComboBox->itemData(i).toString() == result) {
                        m_presetComboBox->setCurrentIndex(i);
                        emit m_presetComboBox->currentIndexChanged(i);
                        return;
                    }
                }
                m_presetComboBox->addItem(result, result);
                m_presetComboBox->setCurrentIndex(m_presetComboBox->count() - 1);
            }
        });
        connect(m_presetDeleteButton, &QAbstractButton::clicked, this, [this] {
            AudioExporter::removePreset(m_presetComboBox->currentData().toString());
            m_presetComboBox->removeItem(m_presetComboBox->currentIndex());
        });

        emit m_fileTypeComboBox->currentIndexChanged(0);
        emit m_sourceComboBox->currentIndexChanged(0);
        emit m_mixingOptionComboBox->currentIndexChanged(0);
        auto currentPresetKey = AudioSettings::audioExporterCurrentPreset();
        if (currentPresetKey.isString()) {
            for (int i = 0; i < m_presetComboBox->count(); i++) {
                if (m_presetComboBox->itemData(i).toString() == currentPresetKey.toString()) {
                    m_presetComboBox->setCurrentIndex(i);
                    break;
                }
            }
        } else {
            m_presetComboBox->setCurrentIndex(currentPresetKey.toInt());
        }
        emit m_presetComboBox->currentIndexChanged(m_presetComboBox->currentIndex());
        if (hasTemporaryPreset()) {
            restoreTemporaryPreset();
            updateView();
        }

        connect(exportButton, &QAbstractButton::clicked, this, &AudioExportDialog::runExport);
        connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    }

    AudioExportDialog::~AudioExportDialog() {
    }

    QStringList AudioExportDialog::projectTrackList() {
        const auto tracks = &appModel->tracks();
        QStringList list;
        for (int i = 0; i < tracks->size(); i++) {
            list.append(tracks->at(i)->name());
        }
        return list;
    }

    void AudioExportDialog::browseFile() {
        const QStringList filters = {
            tr("WAV (*.wav)"),
            tr("FLAC (*.flac)"),
            tr("Ogg Vorbis (*.ogg)"),
            tr("MP3 (*.mp3)"),
        };
        QString selectedFilter = filters.at(m_fileTypeComboBox->currentIndex());
        const auto path =
            QFileDialog::getSaveFileName(this, {},
                                         QDir(m_audioExporter->d_func()->projectDirectory())
                                             .absoluteFilePath(m_fileDirectoryEdit->text()),
                                         filters.join(";;"), &selectedFilter);
        if (path.isEmpty())
            return;
        const QFileInfo fileInfo(path);
        m_fileNameEdit->setText(
            fileInfo.completeBaseName() +
            (m_mixingOptionComboBox->currentIndex() == AudioExporterConfig::MO_Mixed
                 ? "."
                 : "_${trackIndex}_${trackName}.") +
            fileInfo.suffix());
        m_fileDirectoryEdit->setText(fileInfo.dir().canonicalPath());
        m_fileTypeComboBox->setCurrentIndex(filters.indexOf(selectedFilter));
    }

    void AudioExportDialog::showWarnings() {
        QMessageBox::warning(this, {},
                             AudioExporter::warningText(m_audioExporter->warning()).join("\n\n"));
    }

    void AudioExportDialog::showDryRunResult() {
        QDialog dlg(this);
        const auto layout = new QVBoxLayout;
        const auto fileListGroupBox = new QGroupBox(tr("File List"));
        const auto fileListLayout = new QVBoxLayout;
        const auto fileListWidget = new QListWidget;
        fileListWidget->setSelectionMode(QAbstractItemView::NoSelection);
        auto fileList = m_audioExporter->dryRun();
        QHash<QString, int> fileListCountDict;
        for (const auto &file : fileList) {
            fileListCountDict[file]++;
        }
        for (const auto &file : fileList) {
            const auto item = new QListWidgetItem;
            item->setText(file);
            AudioExporter::Warning fileWarning;
            if (QFileInfo::exists(file) && QFileInfo(file).isFile()) {
                fileWarning |= AudioExporter::W_WillOverwrite;
            }
            if (fileListCountDict.value(file) > 1) {
                fileWarning |= AudioExporter::W_DuplicatedFile;
            }
            if (fileWarning) {
                item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                item->setToolTip(AudioExporter::warningText(fileWarning).join('\n'));
            }
            fileListWidget->addItem(item);
        }
        fileListLayout->addWidget(fileListWidget);
        fileListGroupBox->setLayout(fileListLayout);
        layout->addWidget(fileListGroupBox);
        if (m_audioExporter->warning()) {
            const auto warningGroupBox = new QGroupBox(tr("Warnings"));
            const auto warningLayout = new QVBoxLayout;
            const auto warningWidget = new QListWidget;
            warningWidget->setWordWrap(true);
            warningWidget->setSelectionMode(QAbstractItemView::NoSelection);
            for (const auto &warning : AudioExporter::warningText(m_audioExporter->warning())) {
                const auto item = new QListWidgetItem;
                item->setText(warning);
                item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                warningWidget->addItem(item);
            }
            warningLayout->addWidget(warningWidget);
            warningGroupBox->setLayout(warningLayout);
            layout->addWidget(warningGroupBox);
        }
        const auto buttonLayout = new QHBoxLayout();
        const auto okButton = new QPushButton(tr("OK"));
        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        layout->addLayout(buttonLayout);
        dlg.setLayout(layout);
        connect(okButton, &QAbstractButton::clicked, &dlg, &QDialog::accept);
        dlg.setWindowTitle(tr("Dry Run"));
        dlg.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        dlg.resize(600, 400);
        dlg.exec();
    }

    void AudioExportDialog::updateConfig() const {
        if (skipUpdateFlag)
            return;
        AudioExporterConfig config;
        config.setFileName(m_fileNameEdit->text());
        config.setFileDirectory(m_fileDirectoryEdit->text());
        config.setFileType(
            static_cast<AudioExporterConfig::FileType>(m_fileTypeComboBox->currentIndex()));
        config.setFormatMono(m_formatMonoComboBox->currentIndex() == 0);
        config.setFormatOption(m_formatOptionComboBox->currentIndex());
        config.setFormatQuality(m_vbrSlider->value());
        config.setFormatSampleRate(QLocale().toDouble(m_formatSampleRateComboBox->currentText()));
        config.setMixingOption(
            static_cast<AudioExporterConfig::MixingOption>(m_mixingOptionComboBox->currentIndex()));
        config.setMuteSoloEnabled(m_enableMuteSoloCheckBox->isChecked());
        config.setSourceOption(
            static_cast<AudioExporterConfig::SourceOption>(m_sourceComboBox->currentIndex()));
        QList<int> sourceList;
        for (int i = 0; i < m_sourceListWidget->count(); i++) {
            if (m_sourceListWidget->item(i)->isSelected())
                sourceList.append(i);
        }
        config.setSource(sourceList);
        config.setTimeRange(m_rangeSelectAllRadio->isChecked()
                                ? AudioExporterConfig::TR_All
                                : AudioExporterConfig::TR_LoopSection);
        m_audioExporter->setConfig(config);
        if (m_audioExporter->warning()) {
            m_warningButton->show();
        } else {
            m_warningButton->hide();
        }
        const auto fileList = m_audioExporter->dryRun();
        m_fileNamePreviewLabel->setText(fileList.isEmpty() ? QString()
                                                           : QFileInfo(fileList.at(0)).filePath());
    }

    void AudioExportDialog::updateView() {
        skipUpdateFlag = true;
        const auto config = m_audioExporter->config();
        m_fileNameEdit->setText(config.fileName());
        m_fileDirectoryEdit->setText(config.fileDirectory());
        m_fileTypeComboBox->setCurrentIndex(config.fileType());
        m_formatMonoComboBox->setCurrentIndex(config.formatMono() ? 0 : 1);
        m_formatOptionComboBox->setCurrentIndex(config.formatOption());
        m_vbrSlider->setValue(config.formatQuality());
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        m_formatSampleRateComboBox->setCurrentText(locale.toString(config.formatSampleRate()));
        m_mixingOptionComboBox->setCurrentIndex(config.mixingOption());
        m_enableMuteSoloCheckBox->setChecked(config.isMuteSoloEnabled());
        m_sourceComboBox->setCurrentIndex(config.sourceOption());
        m_rangeSelectAllRadio->setChecked(config.timeRange() == AudioExporterConfig::TR_All);
        skipUpdateFlag = false;
        updateConfig();
    }

    void AudioExportDialog::runExport() {
        const auto warning = m_audioExporter->warning();
        if (warning & AudioExporter::W_LossyFormat) {
            if (!askWarningBeforeExport(AudioExporter::W_LossyFormat, true))
                return;
        }
        if (warning & AudioExporter::W_UnrecognizedTemplate) {
            if (!askWarningBeforeExport(AudioExporter::W_UnrecognizedTemplate, true))
                return;
        }
        if (warning & AudioExporter::W_NoFile) {
            if (!askWarningBeforeExport(AudioExporter::W_NoFile))
                return;
        }
        if (warning & AudioExporter::W_WillOverwrite) {
            if (!askWarningBeforeExport(AudioExporter::W_WillOverwrite))
                return;
        }
        if (warning & AudioExporter::W_DuplicatedFile) {
            if (!askWarningBeforeExport(AudioExporter::W_DuplicatedFile))
                return;
        }

        QDialog progressDialog(this);
        const auto layout = new QVBoxLayout;
        layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

        const auto mainPromptLayout = new QHBoxLayout;
        const auto iconLabel = new QLabel;
        iconLabel->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(24, 24));
        iconLabel->setVisible(false);
        mainPromptLayout->addWidget(iconLabel);
        const auto mainPromptLabel = new QLabel(tr("Preparing..."));
        mainPromptLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        mainPromptLayout->addWidget(mainPromptLabel, 1);
        layout->addLayout(mainPromptLayout);

        const auto mainProgressBar = new QProgressBar;
        mainProgressBar->setRange(0, 100);
        mainProgressBar->setValue(0);
        layout->addWidget(mainProgressBar);

        const auto buttonLayout = new QHBoxLayout;
        const auto keepPartialFileCheckBox = new QCheckBox(tr("&Keep partial files"));
        keepPartialFileCheckBox->setVisible(false);
        buttonLayout->addWidget(keepPartialFileCheckBox);
        buttonLayout->addStretch();
        const auto mainPromptWarningButton = new QPushButton;
        mainPromptWarningButton->setVisible(false);
        mainPromptWarningButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
        buttonLayout->addWidget(mainPromptWarningButton, 0);
        const auto abortButton = new QPushButton(tr("Cancel"));
        buttonLayout->addWidget(abortButton);
        layout->addLayout(buttonLayout);

        progressDialog.setLayout(layout);
        progressDialog.resize(400, progressDialog.height());

        bool isProgressing = false;
        if (m_audioExporter->config().mixingOption() == AudioExporterConfig::MO_Mixed) {
            connect(m_audioExporter, &AudioExporter::progressChanged, &progressDialog,
                    [=, &isProgressing](const double ratio) {
                        if (!isProgressing) {
                            isProgressing = true;
                            mainPromptLabel->setText(tr("Exporting..."));
                        }
                        mainProgressBar->setValue(static_cast<int>(ratio * 100.0));
                    });
        } else {
            QHash<int, double> progressRatioHash;
            const int sourceCount = m_audioExporter->config().source().size();
            connect(
                m_audioExporter, &AudioExporter::progressChanged, &progressDialog,
                [=, &progressRatioHash, &isProgressing](const double ratio, const int sourceIndex) {
                    if (!isProgressing) {
                        isProgressing = true;
                        mainPromptLabel->setText(tr("Exporting..."));
                    }
                    progressRatioHash[sourceIndex] = ratio;
                    double totalRatio = 0;
                    for (const auto value : progressRatioHash.values()) {
                        totalRatio += value;
                    }
                    mainProgressBar->setValue(static_cast<int>(totalRatio / sourceCount * 100.0));
                });
        }

        QDialog warningListDialog(this);
        const auto warningListDialogLayout = new QVBoxLayout;
        const auto warningList = new QListWidget;
        warningList->setSelectionMode(QAbstractItemView::NoSelection);
        warningListDialogLayout->addWidget(warningList);
        warningListDialog.setLayout(warningListDialogLayout);
        warningListDialog.resize(300, 300);
        warningListDialog.setWindowTitle(tr("Warnings"));
        connect(m_audioExporter, &AudioExporter::clippingDetected, &progressDialog,
                [=, this](const int sourceIndex) {
                    const auto item = new QListWidgetItem;
                    if (sourceIndex == -1) {
                        item->setText(tr("Clipping is detected"));
                    } else {
                        item->setText(tr("Clipping is detected in track %1 \"%2\"")
                                          .arg(sourceIndex + 1)
                                          .arg(m_audioExporter->d_func()->trackName(sourceIndex)));
                    }
                    item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                    warningList->addItem(item);
                    mainPromptWarningButton->setVisible(true);
                    mainPromptWarningButton->setToolTip(
                        tr("%n warning(s)", nullptr, warningList->count()));
                    QApplication::beep();
                });
        connect(m_audioExporter, &AudioExporter::warningAdded, &progressDialog,
                [=, this](const QString &message, const int sourceIndex) {
                    Q_UNUSED(sourceIndex); // may be used in future
                    const auto item = new QListWidgetItem;
                    item->setText(message);
                    item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                    warningList->addItem(item);
                    mainPromptWarningButton->setVisible(true);
                    mainPromptWarningButton->setToolTip(
                        tr("%n warning(s)", nullptr, warningList->count()));
                    QApplication::beep();
                });

        connect(mainPromptWarningButton, &QAbstractButton::clicked, &progressDialog,
                [=, &warningListDialog] { warningListDialog.exec(); });

        bool interruptFlag = true;
        connect(abortButton, &QAbstractButton::clicked, &progressDialog,
                [=, &interruptFlag, &progressDialog, this] {
                    if (interruptFlag) {
                        m_audioExporter->cancel();
                    } else {
                        progressDialog.reject();
                    }
                });

        class CloseEventFilter : public QObject {
        public:
            CloseEventFilter(QDialog *progressDialog, std::function<bool()> &&cb) : cb(cb) {
                progressDialog->installEventFilter(this);
            }

            std::function<bool()> cb;

            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::Close) {
                    if (cb())
                        event->accept();
                    else
                        event->ignore();
                    return true;
                }
                return QObject::eventFilter(watched, event);
            }
        } o(&progressDialog, [=, &interruptFlag, this] {
            if (interruptFlag) {
                m_audioExporter->cancel();
                return false;
            }
            return true;
        });

        QTimer::singleShot(0, [=, &interruptFlag, &progressDialog, this] {
            QCoreApplication::processEvents();
            const auto ret = m_audioExporter->exec();
            interruptFlag = false;
            abortButton->setText(tr("Close"));
            if (warningList->count()) {
                switch (ret) {
                    case AudioExporter::R_Ok:
                        mainPromptLabel->setText(tr("Export finished with %n warning(s)", nullptr,
                                                    warningList->count()));
                        QApplication::beep();
                        break;
                    case AudioExporter::R_Abort:
                        mainPromptLabel->setText(
                            tr("Export aborted with %n warning(s)", nullptr, warningList->count()));
                        QApplication::beep();
                        keepPartialFileCheckBox->setVisible(true);
                        break;
                    case AudioExporter::R_Fail:
                        mainPromptLabel->setText(tr("Export failed with %n warning(s)\n%1", nullptr,
                                                    warningList->count())
                                                     .arg(m_audioExporter->errorString()));
                        iconLabel->setVisible(true);
                        QApplication::beep();
                        keepPartialFileCheckBox->setVisible(true);
                        break;
                }
            } else {
                switch (ret) {
                    case AudioExporter::R_Ok:
                        progressDialog.accept();
                        break;
                    case AudioExporter::R_Abort:
                        mainPromptLabel->setText(tr("Export aborted"));
                        QApplication::beep();
                        keepPartialFileCheckBox->setVisible(true);
                        break;
                    case AudioExporter::R_Fail:
                        mainPromptLabel->setText(
                            tr("Export failed\n%1").arg(m_audioExporter->errorString()));
                        iconLabel->setVisible(true);
                        QApplication::beep();
                        keepPartialFileCheckBox->setVisible(true);
                        break;
                }
            }
        });
        const auto ret = progressDialog.exec();
        if (!keepPartialFileCheckBox->isChecked()) {
            m_audioExporter->cleanUp();
        }
        const auto currentData = m_presetComboBox->currentData();
        AudioSettings::setAudioExporterCurrentPreset(
            currentData.isNull() ? QJsonValue(m_presetComboBox->currentIndex())
                                 : QJsonValue(currentData.toString()));
        saveTemporaryPreset();
        if (ret == QDialog::Accepted && !m_keepOpenCheckBox->isChecked())
            accept();
        else
            updateView();
    }

    bool AudioExportDialog::askWarningBeforeExport(const AudioExporter::Warning warning,
                                                   const bool canIgnored) {
        if (AudioSettings::audioExporterIgnoredWarningFlag() & warning)
            return true;
        QMessageBox msgBox(this);
        msgBox.setText(AudioExporter::warningText(warning)[0] + "\n\n" + tr("Continue to export?"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        QCheckBox *checkBox = nullptr;
        if (canIgnored) {
            checkBox = new QCheckBox(tr("Don't ask again"));
            msgBox.setCheckBox(checkBox);
        }
        if (msgBox.exec() == QMessageBox::Yes) {
            if (canIgnored && checkBox->isChecked())
                AudioSettings::setAudioExporterIgnoredWarningFlag(
                    AudioSettings::audioExporterIgnoredWarningFlag() | warning);
            return true;
        }
        return false;
    }

    void AudioExportDialog::saveTemporaryPreset() const {
        m_temporaryPreset = m_audioExporter->config();
        m_hasTemporaryPreset = true;
    }

    void AudioExportDialog::restoreTemporaryPreset() const {
        m_audioExporter->setConfig(m_temporaryPreset);
    }

    bool AudioExportDialog::hasTemporaryPreset() {
        return m_hasTemporaryPreset;
    }


}
