#include "tst_application_gui.h"

#include "../TestSupport/MainWindowFixture.h"
#include "../TestSupport/VoicebankFixture.h"
#include "../TestSupport/WaveFixture.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Audio/AudioExporter.h"
#include "Modules/ProjectConverters/MidiBatchImportDialog.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"
#include "Modules/ProjectConverters/MidiConverterDialog.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Dialogs/Audio/AudioExportProgressDialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Dialogs/Extractor/ExtractPitchParamDialog.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"
#include "UI/Dialogs/Help/AboutDialog.h"
#include "UI/Dialogs/Note/PhonemeEditorDialog.h"
#include "UI/Dialogs/Note/QuantizeDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/PackageManager/PackageManagerDialog.h"
#include "UI/Dialogs/ResourceCheck/AudioResourcePage.h"
#include "UI/Dialogs/ResourceCheck/ResourceCheckDialog.h"
#include "UI/Dialogs/Search/SearchDialog.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include "UI/Dialogs/Timeline/EditTempoDialog.h"
#include "UI/Dialogs/Timeline/EditTimeSignatureDialog.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/MainTitleBar/FilePopupWidget.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Window/DetachedPanelWindow.h"
#include "UI/Window/LogWindow.h"
#include "UI/Window/TaskWindow.h"

#include <lite/GUI/Utils/WindowFrameUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QListWidget>
#include <QPainter>
#include <QScopeGuard>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>

#include <cmath>
#include <numbers>

namespace {
    class Gallery {
    public:
        explicit Gallery(const QString &outputDirectory) : m_output(outputDirectory) {
        }

        void addReference(const QString &title, const QString &name) {
            addImage(title, m_output.filePath(name));
        }

        void setHost(QWidget &window) {
            m_host = &window;
        }

        void take(QWidget &window, const QString &title) {
            QVERIFY2(QTest::qWaitForWindowExposed(&window), qPrintable(title));
            QTest::qWait(100);
            const auto number = QStringLiteral("%1").arg(m_entries.size() + 1, 2, 10,
                                                         QLatin1Char('0'));
            const auto path = m_output.filePath(
                QStringLiteral("gallery/%1.png").arg(number));
            qInfo().noquote() << "Capturing" << title << "to" << path;

            // Capture each native frame in-process, then place the transient over
            // its complete editor host. A dialog-only grab hides the very border
            // and title-bar relationships this gallery is meant to review.
            auto *host = m_host;
            for (auto *parent = window.parentWidget(); parent; parent = parent->parentWidget()) {
                if (qobject_cast<MainWindow *>(parent) ||
                    qobject_cast<DetachedPanelWindow *>(parent)) {
                    host = parent;
                    break;
                }
            }
            const bool isEditor = qobject_cast<MainWindow *>(&window) ||
                                  qobject_cast<DetachedPanelWindow *>(&window);
            if (!host || host == &window || isEditor) {
                QVERIFY2(WindowFrameUtils::captureWindowChrome(&window, path), qPrintable(title));
            } else {
                QVERIFY2(host->isVisible(), qPrintable(title));
                QVERIFY(m_output.mkpath(QStringLiteral("gallery/layers")));
                const auto hostPath = m_output.filePath(
                    QStringLiteral("gallery/layers/%1-host.png").arg(number));
                const auto targetPath = m_output.filePath(
                    QStringLiteral("gallery/layers/%1-target.png").arg(number));
                QVERIFY2(WindowFrameUtils::captureWindowChrome(host, hostPath), qPrintable(title));
                QVERIFY2(WindowFrameUtils::captureWindowChrome(&window, targetPath), qPrintable(title));
                QImage hostImage(hostPath);
                QImage targetImage(targetPath);
                QVERIFY(!hostImage.isNull() && !targetImage.isNull());
                const auto bounds = [](QWidget &widget, const QImage &image) {
                    // Cocoa captures the native frame. Other platforms currently
                    // return QWidget::grab(), whose origin is the client area.
                    const auto origin = QGuiApplication::platformName() == QStringLiteral("cocoa")
                                            ? widget.frameGeometry().topLeft()
                                            : widget.mapToGlobal(QPoint());
                    return QRectF(origin, QSizeF(image.size()) / widget.devicePixelRatioF());
                };
                const auto hostRect = bounds(*host, hostImage);
                const auto targetRect = bounds(window, targetImage);
                const auto sceneRect = hostRect.united(targetRect).adjusted(-12, -12, 12, 12);
                const auto dpr = qMax(host->devicePixelRatioF(), window.devicePixelRatioF());
                QImage scene((sceneRect.size() * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
                QVERIFY(!scene.isNull());
                scene.setDevicePixelRatio(dpr);
                scene.fill(QColor("#edf0f4"));
                QPainter painter(&scene);
                painter.drawImage(hostRect.translated(-sceneRect.topLeft()), hostImage);
                painter.drawImage(targetRect.translated(-sceneRect.topLeft()), targetImage);
                painter.end();
                QVERIFY(scene.save(path));
                QVERIFY(sceneRect.contains(hostRect) && sceneRect.contains(targetRect));
                qInfo().noquote() << "Full host" << hostRect << "with transient" << targetRect
                                  << "in" << sceneRect;
            }
            addImage(title, path);
        }

        void show(QWidget &window, const QString &title) {
            window.show();
            window.activateWindow();
            take(window, title);
            window.hide();
        }

        void writeSheet() const {
            if (m_entries.isEmpty())
                return;
            constexpr int columns = 3;
            constexpr int gap = 24;
            constexpr int captionHeight = 40;
            int columnWidth = 0;
            QList<int> rowHeights((m_entries.size() + columns - 1) / columns, 0);
            for (qsizetype i = 0; i < m_entries.size(); ++i) {
                const auto &image = m_entries.at(i).image;
                columnWidth = qMax(columnWidth, image.width());
                auto &height = rowHeights[i / columns];
                height = qMax(height, image.height());
            }
            int sheetHeight = gap;
            for (const int height : rowHeights)
                sheetHeight += height + captionHeight + gap;
            QImage sheet(columns * columnWidth + (columns + 1) * gap, sheetHeight,
                         QImage::Format_RGB32);
            QVERIFY(!sheet.isNull());
            sheet.fill(QColor("#edf0f4"));

            QPainter painter(&sheet);
            QFont font = painter.font();
            font.setPixelSize(22);
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(QColor("#182433"));
            int y = gap;
            for (qsizetype i = 0; i < m_entries.size(); ++i) {
                const auto &entry = m_entries.at(i);
                const int x = gap + static_cast<int>(i % columns) * (columnWidth + gap);
                const auto caption = QStringLiteral("%1. %2").arg(i + 1).arg(entry.title);
                painter.drawText(QRect(x, y, columnWidth, captionHeight),
                                 Qt::AlignLeft | Qt::AlignVCenter, caption);
                painter.drawImage(x, y + captionHeight, entry.image);
                if (i % columns == columns - 1)
                    y += rowHeights.at(i / columns) + captionHeight + gap;
            }
            painter.end();
            QVERIFY(sheet.save(m_output.filePath(QStringLiteral("ui-review.png"))));
            qInfo("Wrote %lld labelled screenshots to ui-review.png",
                  static_cast<long long>(m_entries.size()));
        }

    private:
        void addImage(const QString &title, const QString &path) {
            QImage image(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            constexpr QSize maximumSize(1200, 800);
            if (image.width() > maximumSize.width() || image.height() > maximumSize.height())
                image = image.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_entries.append({title, std::move(image)});
        }

        struct Entry {
            QString title;
            QImage image;
        };

        QDir m_output;
        QWidget *m_host = nullptr;
        QList<Entry> m_entries;
    };
} // namespace

void ApplicationGuiTests::captureUiGallery() {
    const auto output = qEnvironmentVariable("LITE_UI_GALLERY");
    if (output.isEmpty())
        QSKIP("Set LITE_UI_GALLERY to generate the UI review artifact");
    QVERIFY(QDir(output).mkpath(QStringLiteral("gallery")));
    Gallery gallery(output);
    const auto compose = qScopeGuard([&] { gallery.writeSheet(); });
    if (QFileInfo::exists(QDir(output).filePath(QStringLiteral("title-bar-native-frame.png")))) {
        gallery.addReference(QStringLiteral("Main window - system frame"),
                             QStringLiteral("title-bar-native-frame.png"));
        gallery.addReference(QStringLiteral("Main window - frameless"),
                             QStringLiteral("title-bar-frameless.png"));
    }

    TestSupport::MainWindowFixture host(TestSupport::MainWindowFixture::MenuPlacement::PlatformDefault,
                                        false);
    createLyricSelection(480, true);
    if (QTest::currentTestFailed())
        return;
    view->setDataContext(nullptr);
    view.reset();
    scene.reset();
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto &window = *host.window;
    gallery.setHost(window);
    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *editor = bottom->clipEditorView();

    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto audioTrack;
    audioTrack.name = QStringLiteral("Guide audio");
    const auto track = runtime.project().insertTrack(commandContext(), 1, audioTrack);
    QVERIFY(track);
    Automation::ClipDraftDto audio;
    audio.type = Automation::ClipDraftDto::Type::Audio;
    audio.properties.name = QStringLiteral("Guide waveform");
    audio.properties.start = 960;
    audio.properties.length = 1920;
    audio.properties.clipLen = 1920;
    audio.audioPath = dataRoot.filePath(QStringLiteral("gallery.wav"));
    constexpr int sampleRate = 8000;
    QVector<float> wave(4 * sampleRate);
    for (qsizetype i = 0; i < wave.size(); ++i) {
        const double time = static_cast<double>(i) / sampleRate;
        const double envelope = 0.25 + 0.2 * std::sin(2 * std::numbers::pi * 2 * time);
        wave[i] = static_cast<float>(envelope * std::sin(2 * std::numbers::pi * 220 * time));
    }
    QVERIFY(TestSupport::writeWave(audio.audioPath, wave, 1, sampleRate));
    const auto inserted = runtime.project().insertClips(
        commandContext(), {{.trackId = Automation::TrackId(track.get().affectedObjects.first().value),
                            .clip = audio}});
    QVERIFY(inserted);
    auto *audioClip = dynamic_cast<AudioClip *>(
        context->m_appModel->findClipById(inserted.get().affectedObjects.first().value));
    QVERIFY(audioClip);

    QVERIFY(window.setEditorPanelVisibility(true, false));
    gallery.take(window, QStringLiteral("Track editor"));
    auto *projectTitle = window.findChild<TitleBarComboBox *>();
    QVERIFY(projectTitle && projectTitle->popupWidget());
    projectTitle->showPopup();
    gallery.take(*projectTitle->popupWidget(), QStringLiteral("Project popup with full main window"));
    projectTitle->hidePopup();
    for (const auto *name : {"actionTempoLane", "actionTimeSignatureLane"}) {
        auto *action = window.findChild<QAction *>(name);
        QVERIFY(action);
        if (!action->isChecked())
            action->trigger();
    }
    gallery.take(window, QStringLiteral("Track editor - tempo and time signature lanes"));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    editor->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(editor->hasActiveSingingClip());
    QVERIFY(editor->setRegionVisibility(true, true));
    QVERIFY(window.centerPianoRollAt(1440, 60));
    gallery.take(window, QStringLiteral("Track editor, piano roll and parameters"));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    gallery.take(window, QStringLiteral("Expanded clip editor"));
    const auto foreground = editor->parameterViewState().foreground;
    QVERIFY(editor->setParameterForeground(ParamInfo::SpeakerMix));
    gallery.take(window, QStringLiteral("Dynamic speaker mix"));
    QVERIFY(editor->setParameterForeground(foreground));
    editor->onActiveClipChanged(audioClip->id());
    gallery.take(window, QStringLiteral("Audio clip waveform"));
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("MixConsole")));
    gallery.take(window, QStringLiteral("Mixer"));

    emit bottom->detachRequested();
    auto *detached = window.findChild<DetachedPanelWindow *>();
    QVERIFY(detached && detached->isVisible());
    gallery.take(*detached, QStringLiteral("Detached mixer"));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    gallery.take(*detached, QStringLiteral("Detached piano roll and parameters"));
    {
        QuantizeDialog quantize(detached);
        gallery.show(quantize, QStringLiteral("Detached editor - quantize notes"));
        SearchDialog search(singingClip, detached);
        gallery.show(search, QStringLiteral("Detached editor - search lyrics"));
    }
    detached->close();
    QVERIFY(!detached->isVisible());
    QVERIFY(window.setEditorPanelVisibility(true, true));

    LogWindow log(&window);
    gallery.show(log, QStringLiteral("Log viewer"));
    TaskWindow tasks(&window);
    gallery.show(tasks, QStringLiteral("Task list"));

    // Use the real modal wrapper, then step through its sidebar before dismissing it.
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<Dialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto dismiss = qScopeGuard([&] { dialog->reject(); });
        auto *sidebar = dialog->findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(sidebar && sidebar->count() > 0);
        for (int page = 0; page < sidebar->count(); ++page) {
            sidebar->setCurrentRow(page);
            gallery.take(*dialog, QStringLiteral("Options - %1").arg(sidebar->item(page)->text()));
        }
        sidebar->setCurrentRow(AppOptionsGlobal::Automation - 1);
        auto *toolset = dialog->findChild<QAbstractButton *>("automationOpenToolsetButton");
        QVERIFY(toolset && toolset->isEnabled());
        toolset->click();
        gallery.take(*dialog, QStringLiteral("Options - Automation custom toolset"));
        const QString prefix = QStringLiteral("automationCustomToolGroupExpand_");
        for (auto *button : dialog->findChildren<QAbstractButton *>()) {
            if (!button->objectName().startsWith(prefix))
                continue;
            for (auto *parent = button->parentWidget(); parent; parent = parent->parentWidget()) {
                if (auto *scroll = qobject_cast<QScrollArea *>(parent)) {
                    scroll->ensureWidgetVisible(button);
                    break;
                }
            }
            button->click();
            gallery.take(*dialog, QStringLiteral("Custom toolset - %1")
                                      .arg(button->objectName().sliced(prefix.size())));
            button->click();
        }
    });
    window.openAppOptions(AppOptionsGlobal::General);
    if (QTest::currentTestFailed())
        return;

    const auto show = [&](QWidget &&dialog, const QString &title) { gallery.show(dialog, title); };
    show(AboutDialog(&window), QStringLiteral("About"));
    {
        PackageManagerDialog packages(&window);
        packages.show();
        packages.activateWindow();
        gallery.take(packages, QStringLiteral("Package manager"));
        auto *list = packages.findChild<QListView *>("PackageManagerDialogPackageListView");
        QVERIFY(list && list->model());
        QTRY_VERIFY(list->model()->rowCount() > 0);
        list->setCurrentIndex(list->model()->index(0, 0));
        gallery.take(packages, QStringLiteral("Package manager - package details"));
    }
    show(Audio::Internal::AudioExportDialog(&window), QStringLiteral("Export audio - configuration"));
    Audio::AudioExporter exporter(nullptr);
    show(Audio::Internal::AudioExportProgressDialog(&exporter, &window),
         QStringLiteral("Export audio - progress"));

    const QList<MidiImportTrackInfo> midiTracks{
        {.name = "Lead", .rangeText = QStringLiteral("C4 - E4"), .noteCount = 3,
         .lyrics = {"la", "la", "la"}, .selectedByDefault = true}};
    ProjectImportConfigDialog projectImport(&window);
    projectImport.setPage(new MidiConfigPage(midiTracks));
    gallery.show(projectImport, QStringLiteral("Project import - MIDI options"));
    show(MidiConverterDialog(midiTracks, &window), QStringLiteral("MIDI converter"));
    show(MidiBatchImportDialog("UTF-8", &window), QStringLiteral("Batch MIDI import"));
    show(ExtractPitchParamDialog({audioClip}), QStringLiteral("Extract pitch - source selection"));
    ResourceCheckDialog resources(&window);
    resources.addPage(new AudioResourcePage({}, {audioClip->id()}));
    gallery.show(resources, QStringLiteral("Resource check - audio files"));

    show(EditTempoDialog(480, 120.0, &window), QStringLiteral("Edit tempo"));
    show(EditTimeSignatureDialog(1, 4, 4, &window), QStringLiteral("Edit time signature"));
    show(QuantizeDialog(&window), QStringLiteral("Quantize notes"));
    show(SearchDialog(singingClip, &window), QStringLiteral("Search lyrics"));
    QList<Note *> notes;
    for (auto *note : singingClip->notes())
        notes.append(note);
    QVERIFY(!notes.isEmpty());
    show(PhonemeEditorDialog(notes.first(), &window), QStringLiteral("Edit phonemes"));
    {
        LyricDialog lyrics(singingClip, notes, singingClip->singerIdentifier(),
                           {TestSupport::fixtureLanguage()}, &window);
        lyrics.show();
        lyrics.activateWindow();
        gallery.take(lyrics, QStringLiteral("Fill lyrics"));
        for (auto *button : lyrics.findChildren<QAbstractButton *>()) {
            if (button->text() == QStringLiteral("Lyric Prev") ||
                button->text() == QStringLiteral("Fold Preview")) {
                button->click();
                gallery.take(lyrics, QStringLiteral("Fill lyrics - preview toggled"));
                break;
            }
        }
        auto *tabs = lyrics.findChild<QTabWidget *>();
        QVERIFY(tabs && tabs->count() > 1);
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            gallery.take(lyrics, QStringLiteral("Fill lyrics - %1").arg(tabs->tabText(i)));
        }
    }
    const SingerInfo singer(
        {QStringLiteral("mix-test"), QStringLiteral("gallery"), QVersionNumber(1, 0)},
        QStringLiteral("Demo voice"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm"))});
    show(SpeakerMixDialog(singer, {}, &window), QStringLiteral("Speaker mix"));

    MessageDialog message(QStringLiteral("Save changes?"),
                          QStringLiteral("The current project has unsaved changes."), &window);
    message.addAccentButton(QStringLiteral("Save"), 0);
    message.addButton(QStringLiteral("Discard"), 1);
    message.addButton(QStringLiteral("Cancel"), 2);
    gallery.show(message, QStringLiteral("Confirmation dialog"));
    TaskDialog task(nullptr, true, true, &window);
    task.setTitle(QStringLiteral("Processing project"));
    task.setMessage(QStringLiteral("Preparing audio resources"));
    task.setProgressRange(0, 100);
    task.setProgressValue(45);
    gallery.show(task, QStringLiteral("Task progress"));
    show(RestartDialog(QStringLiteral("Restart the editor to apply the settings."), true, &window),
         QStringLiteral("Restart prompt"));
}
