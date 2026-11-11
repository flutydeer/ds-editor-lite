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
#include "UI/Views/TrackEditor/TrackEditorView.h"
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
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPainter>
#include <QScopeGuard>
#include <QTabWidget>
#include <QTimer>
#include <QTemporaryDir>

#include <cmath>
#include <numbers>

namespace {
    enum class Module {
        Workspace,
        Tracks,
        Notes,
        Lyrics,
        Mixing,
        Audio,
        Packages,
        Settings,
        Diagnostics
    };

    constexpr Module modules[]{Module::Workspace, Module::Tracks,   Module::Notes,
                               Module::Lyrics,    Module::Mixing,   Module::Audio,
                               Module::Packages,  Module::Settings, Module::Diagnostics};

    QString moduleTitle(const Module module) {
        switch (module) {
            case Module::Workspace:
                return QStringLiteral("Project & workspace");
            case Module::Tracks:
                return QStringLiteral("Tracks & timeline");
            case Module::Notes:
                return QStringLiteral("Notes & pitch editing");
            case Module::Lyrics:
                return QStringLiteral("Lyrics & pronunciation");
            case Module::Mixing:
                return QStringLiteral("Voice & mixing");
            case Module::Audio:
                return QStringLiteral("Audio & project I/O");
            case Module::Packages:
                return QStringLiteral("Packages & resources");
            case Module::Settings:
                return QStringLiteral("Preferences");
            case Module::Diagnostics:
                return QStringLiteral("Tasks & diagnostics");
        }
        Q_UNREACHABLE();
    }

    QJsonArray rectJson(const QRect &rect) {
        return {rect.x(), rect.y(), rect.width(), rect.height()};
    }

    class Gallery {
    public:
        enum class Capture { NativeWindow, WidgetContent };

        explicit Gallery(const QString &outputDirectory) : m_output(outputDirectory) {
        }

        void addReference(const Module module, const QString &title, const QString &name) {
            QFile report(m_output.filePath(QFileInfo(name).completeBaseName() + ".json"));
            QString method = QStringLiteral("reference-image");
            if (report.open(QIODevice::ReadOnly)) {
                method = QJsonDocument::fromJson(report.readAll())
                             .object()
                             .value(QStringLiteral("captureMethod"))
                             .toString(method);
            }
            addImage(module, title, m_output.filePath(name), method);
        }

        void take(QWidget &widget, const Module module, const QString &title,
                  const Capture capture = Capture::NativeWindow) {
            QVERIFY2(QTest::qWaitForWindowExposed(widget.window()), qPrintable(title));
            QTRY_VERIFY(widget.isVisible());
            QTest::qWait(150);
            QVERIFY(m_output.mkpath(QStringLiteral("gallery")));
            const auto number =
                QStringLiteral("%1").arg(m_entries.size() + 1, 2, 10, QLatin1Char('0'));
            const auto path = m_output.filePath(QStringLiteral("gallery/%1.png").arg(number));
            QString method;
            if (capture == Capture::WidgetContent) {
                // Custom editor panels and popups do not need a repeated host window.
                // Keep exactly their rendered contents, without adding decorative chrome.
                QVERIFY2(widget.grab().save(path), qPrintable(title));
                method = QStringLiteral("qt-widget-content");
            } else {
                QVERIFY2(widget.isWindow(), qPrintable(title));
                QVERIFY2(WindowFrameUtils::captureWindowChrome(&widget, path, &method),
                         qPrintable(title));
            }
            qInfo().noquote() << "Captured" << title << "using" << method << "to" << path;
            addImage(module, title, path, method);
        }

        void show(QWidget &window, const Module module, const QString &title) {
            window.show();
            window.activateWindow();
            take(window, module, title);
            window.hide();
        }

        void writeSheet() const {
            if (m_entries.isEmpty())
                return;
            constexpr int sheetWidth = 2560;
            constexpr int margin = 64;
            constexpr int columnGap = 48;
            constexpr int rowGap = 44;
            constexpr int sectionGap = 64;
            constexpr int headingHeight = 52;
            constexpr int headingGap = 24;
            constexpr int captionGap = 12;
            constexpr int captionHeight = 54;
            constexpr int columnWidth = (sheetWidth - 2 * margin - columnGap) / 2;

            struct Placement {
                const Entry *entry;
                QRect image;
                QRect caption;
            };

            struct Section {
                Module module;
                QRect heading;
                QRect bounds;
                QList<Placement> images;
            };

            QList<Section> sections;
            int y = margin;
            for (const auto module : modules) {
                QList<const Entry *> entries;
                for (const auto &entry : m_entries) {
                    if (entry.module == module)
                        entries.append(&entry);
                }
                if (entries.isEmpty())
                    continue;
                const int sectionTop = y;
                Section section{
                    module, QRect(margin, y, sheetWidth - 2 * margin, headingHeight), {}, {}};
                y += headingHeight + headingGap;
                for (qsizetype first = 0; first < entries.size(); first += 2) {
                    int rowHeight = 0;
                    for (qsizetype column = 0; column < 2 && first + column < entries.size();
                         ++column) {
                        const auto *entry = entries.at(first + column);
                        const int x = margin + static_cast<int>(column) * (columnWidth + columnGap);
                        const int imageX = x + (columnWidth - entry->image.width()) / 2;
                        section.images.append({entry, QRect(QPoint(imageX, y), entry->image.size()),
                                               QRect(x, y + entry->image.height() + captionGap,
                                                     columnWidth, captionHeight)});
                        rowHeight =
                            qMax(rowHeight, entry->image.height() + captionGap + captionHeight);
                    }
                    y += rowHeight + rowGap;
                }
                y -= rowGap;
                section.bounds = QRect(0, sectionTop - 24, sheetWidth, y - sectionTop + 48);
                sections.append(std::move(section));
                y += sectionGap;
            }
            QImage sheet(sheetWidth, y - sectionGap + margin, QImage::Format_RGB32);
            QVERIFY(!sheet.isNull());
            sheet.fill(QColor("#f1f3f6"));
            QPainter painter(&sheet);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.setPen(QColor("#182433"));
            QFont headingFont = painter.font();
            headingFont.setPixelSize(40);
            headingFont.setBold(true);
            QFont captionFont = headingFont;
            captionFont.setPixelSize(22);
            captionFont.setBold(false);
            QJsonArray sectionReports;
            for (const auto &section : sections) {
                painter.setFont(headingFont);
                // A section has only a full-width heading. No rule or explanatory subtitle.
                painter.drawText(section.heading, Qt::AlignLeft | Qt::AlignVCenter,
                                 moduleTitle(section.module));
                painter.setFont(captionFont);
                QJsonArray imageReports;
                for (const auto &placement : section.images) {
                    const auto &entry = *placement.entry;
                    painter.drawImage(placement.image.topLeft(), entry.image);
                    painter.drawText(placement.caption,
                                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                                     entry.title);
                    imageReports.append(QJsonObject{
                        {"caption",       entry.title                                   },
                        {"file",          m_output.relativeFilePath(entry.path)         },
                        {"captureMethod", entry.captureMethod                           },
                        {"sourceSize",
                         QJsonArray{entry.sourceSize.width(), entry.sourceSize.height()}},
                        {"imageRect",     rectJson(placement.image)                     },
                        {"captionRect",   rectJson(placement.caption)                   }
                    });
                }
                sectionReports.append(QJsonObject{
                    {"title", moduleTitle(section.module)},
                    {"file", QStringLiteral("gallery/modules/%1.png")
                                 .arg(sectionReports.size() + 1, 2, 10, QLatin1Char('0'))},
                    {"headingRect", rectJson(section.heading)},
                    {"images", imageReports}
                });
            }
            painter.end();
            QVERIFY(sheet.save(m_output.filePath(QStringLiteral("ui-review.png"))));
            QVERIFY(m_output.mkpath(QStringLiteral("gallery/modules")));
            for (qsizetype i = 0; i < sections.size(); ++i) {
                const auto path = m_output.filePath(
                    QStringLiteral("gallery/modules/%1.png").arg(i + 1, 2, 10, QLatin1Char('0')));
                QVERIFY(sheet.copy(sections.at(i).bounds).save(path));
            }
            const QJsonObject report{
                {"complete", !QTest::currentTestFailed()              },
                {"size",     QJsonArray{sheet.width(), sheet.height()}},
                {"sections", sectionReports                           }
            };
            QFile manifest(m_output.filePath(QStringLiteral("ui-review.json")));
            QVERIFY(manifest.open(QIODevice::WriteOnly));
            const auto json = QJsonDocument(report).toJson();
            QCOMPARE(manifest.write(json), json.size());
            qInfo("Wrote %lld screenshots in %lld UI modules to ui-review.png",
                  static_cast<long long>(m_entries.size()),
                  static_cast<long long>(sections.size()));
        }

    private:
        void addImage(const Module module, const QString &title, const QString &path,
                      const QString &captureMethod) {
            QImage image(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            const auto sourceSize = image.size();
            // Two broad image columns give panels room without stretching small dialogs.
            constexpr QSize maximumSize(1168, 850);
            if (image.width() > maximumSize.width() || image.height() > maximumSize.height())
                image = image.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            image.setDevicePixelRatio(1);
            m_entries.append({module, title, path, captureMethod, sourceSize, std::move(image)});
        }

        struct Entry {
            Module module;
            QString title;
            QString path;
            QString captureMethod;
            QSize sourceSize;
            QImage image;
        };

        QDir m_output;
        QList<Entry> m_entries;
    };
} // namespace

void ApplicationGuiTests::uiGalleryLayoutGroupsModules() {
    QTemporaryDir output;
    QVERIFY(output.isValid());
    QImage wide(1500, 700, QImage::Format_RGB32);
    wide.fill(Qt::white);
    QImage tall(600, 1000, QImage::Format_RGB32);
    tall.fill(Qt::gray);
    QVERIFY(wide.save(output.filePath("wide.png")));
    QVERIFY(tall.save(output.filePath("tall.png")));
    Gallery gallery(output.path());
    gallery.addReference(Module::Workspace, QStringLiteral("Wide workspace"), "wide.png");
    gallery.addReference(Module::Tracks, QStringLiteral("Track editor"), "wide.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Portrait workspace"), "tall.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Third workspace"), "wide.png");
    gallery.writeSheet();
    if (QTest::currentTestFailed())
        return;
    QFile file(output.filePath("ui-review.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto report = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(report.value("complete").toBool());
    const auto sections = report.value("sections").toArray();
    QCOMPARE(sections.size(), 2);
    QCOMPARE(sections[0].toObject().value("title").toString(), moduleTitle(Module::Workspace));
    QCOMPARE(sections[1].toObject().value("title").toString(), moduleTitle(Module::Tracks));
    const auto rect = [](const QJsonValue &value) {
        const auto a = value.toArray();
        return QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
    };
    int previousBottom = -1;
    for (const auto &sectionValue : sections) {
        const auto section = sectionValue.toObject();
        const auto heading = rect(section.value("headingRect"));
        QVERIFY(heading.top() > previousBottom);
        QCOMPARE(heading.left(), 64);
        QCOMPARE(heading.width(), 2432);
        const auto images = section.value("images").toArray();
        QList<QRect> occupied;
        for (const auto &imageValue : images) {
            const auto image = imageValue.toObject();
            const auto pixels = rect(image.value("imageRect"));
            const auto caption = rect(image.value("captionRect"));
            QVERIFY(pixels.top() > heading.bottom());
            QVERIFY(caption.top() > pixels.bottom());
            QVERIFY(pixels.width() <= 1168);
            QVERIFY(pixels.height() <= 850);
            const auto bounds = pixels.united(caption);
            for (const auto &other : occupied)
                QVERIFY(!bounds.intersects(other));
            occupied.append(bounds);
            previousBottom = qMax(previousBottom, bounds.bottom());
        }
    }
    const auto workspace = sections[0].toObject().value("images").toArray();
    QCOMPARE(workspace.size(), 3);
    QCOMPARE(rect(workspace[0].toObject().value("imageRect")).top(),
             rect(workspace[1].toObject().value("imageRect")).top());
    QVERIFY(rect(workspace[2].toObject().value("imageRect")).top() >
            rect(workspace[1].toObject().value("captionRect")).bottom());
    QVERIFY(!QImage(output.filePath("ui-review.png")).isNull());
    QVERIFY(!QImage(output.filePath("gallery/modules/01.png")).isNull());
}

void ApplicationGuiTests::captureUiGallery() {
    const auto output = qEnvironmentVariable("LITE_UI_GALLERY");
    if (output.isEmpty())
        QSKIP("Set LITE_UI_GALLERY to generate the UI review artifact");
    QVERIFY(QDir(output).mkpath(QStringLiteral("gallery")));
    Gallery gallery(output);
    const auto compose = qScopeGuard([&] { gallery.writeSheet(); });
    if (QFileInfo::exists(QDir(output).filePath(QStringLiteral("title-bar-native-frame.png")))) {
        gallery.addReference(Module::Workspace, QStringLiteral("Main window - system frame"),
                             QStringLiteral("title-bar-native-frame.png"));
        gallery.addReference(Module::Workspace, QStringLiteral("Main window - frameless"),
                             QStringLiteral("title-bar-frameless.png"));
    }

    TestSupport::MainWindowFixture host(
        TestSupport::MainWindowFixture::MenuPlacement::PlatformDefault, false);
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
    auto *trackEditor = window.findChild<TrackEditorView *>();
    QVERIFY(trackEditor);
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
        commandContext(),
        {
            {.trackId = Automation::TrackId(track.get().affectedObjects.first().value),
             .clip = audio}
    });
    QVERIFY(inserted);
    auto *audioClip = dynamic_cast<AudioClip *>(
        context->m_appModel->findClipById(inserted.get().affectedObjects.first().value));
    QVERIFY(audioClip);

    QVERIFY(window.setEditorPanelVisibility(true, false));
    gallery.take(*trackEditor, Module::Tracks, QStringLiteral("Track arrangement"),
                 Gallery::Capture::WidgetContent);
    auto *projectTitle = window.findChild<TitleBarComboBox *>();
    QVERIFY(projectTitle && projectTitle->popupWidget());
    projectTitle->showPopup();
    gallery.take(*projectTitle->popupWidget(), Module::Workspace,
                 QStringLiteral("Project switcher"), Gallery::Capture::WidgetContent);
    projectTitle->hidePopup();
    for (const auto *name : {"actionTempoLane", "actionTimeSignatureLane"}) {
        auto *action = window.findChild<QAction *>(name);
        QVERIFY(action);
        if (!action->isChecked())
            action->trigger();
    }
    gallery.take(*trackEditor, Module::Tracks, QStringLiteral("Tempo and time signature lanes"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    editor->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(editor->hasActiveSingingClip());
    QVERIFY(editor->setRegionVisibility(true, true));
    QVERIFY(window.centerPianoRollAt(1440, 60));
    gallery.take(window, Module::Workspace, QStringLiteral("Editing workspace"));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    gallery.take(*bottom, Module::Notes, QStringLiteral("Piano roll and parameters"),
                 Gallery::Capture::WidgetContent);
    const auto foreground = editor->parameterViewState().foreground;
    QVERIFY(editor->setParameterForeground(ParamInfo::SpeakerMix));
    gallery.take(*bottom, Module::Mixing, QStringLiteral("Dynamic speaker mix"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(editor->setParameterForeground(foreground));
    editor->onActiveClipChanged(audioClip->id());
    gallery.take(*bottom, Module::Audio, QStringLiteral("Audio waveform"),
                 Gallery::Capture::WidgetContent);
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("MixConsole")));
    gallery.take(*bottom, Module::Mixing, QStringLiteral("Mix console"),
                 Gallery::Capture::WidgetContent);

    emit bottom->detachRequested();
    auto *detached = window.findChild<DetachedPanelWindow *>();
    QVERIFY(detached && detached->isVisible());
    gallery.take(*detached, Module::Mixing, QStringLiteral("Detached mix console"));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    gallery.take(*detached, Module::Notes, QStringLiteral("Detached piano roll and parameters"));
    {
        QuantizeDialog quantize(detached);
        gallery.show(quantize, Module::Notes, QStringLiteral("Quantize notes in detached editor"));
        SearchDialog search(singingClip, detached);
        gallery.show(search, Module::Lyrics, QStringLiteral("Search lyrics in detached editor"));
    }
    detached->close();
    QVERIFY(!detached->isVisible());
    QVERIFY(window.setEditorPanelVisibility(true, true));

    LogWindow log(&window);
    gallery.show(log, Module::Diagnostics, QStringLiteral("Log viewer"));
    TaskWindow tasks(&window);
    gallery.show(tasks, Module::Diagnostics, QStringLiteral("Task list"));

    // Use the real modal wrapper, then step through its sidebar before dismissing it.
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<Dialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto dismiss = qScopeGuard([&] { dialog->reject(); });
        auto *sidebar = dialog->findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(sidebar && sidebar->count() > 0);
        for (int page = 0; page < sidebar->count(); ++page) {
            sidebar->setCurrentRow(page);
            gallery.take(*dialog, Module::Settings,
                         QStringLiteral("%1").arg(sidebar->item(page)->text()));
        }
        sidebar->setCurrentRow(AppOptionsGlobal::Automation - 1);
        auto *toolset = dialog->findChild<QAbstractButton *>("automationOpenToolsetButton");
        QVERIFY(toolset && toolset->isEnabled());
        toolset->click();
        gallery.take(*dialog, Module::Settings, QStringLiteral("Automation custom toolset"));
    });
    window.openAppOptions(AppOptionsGlobal::General);
    if (QTest::currentTestFailed())
        return;

    const auto show = [&](QWidget &&dialog, const Module module, const QString &title) {
        gallery.show(dialog, module, title);
    };
    show(AboutDialog(&window), Module::Workspace, QStringLiteral("About"));
    {
        PackageManagerDialog packages(&window);
        packages.show();
        packages.activateWindow();
        gallery.take(packages, Module::Packages, QStringLiteral("Installed packages"));
        auto *list = packages.findChild<QListView *>("PackageManagerDialogPackageListView");
        QVERIFY(list && list->model());
        QTRY_VERIFY(list->model()->rowCount() > 0);
        list->setCurrentIndex(list->model()->index(0, 0));
        gallery.take(packages, Module::Packages, QStringLiteral("Package details"));
    }
    show(Audio::Internal::AudioExportDialog(&window), Module::Audio,
         QStringLiteral("Export audio - configuration"));
    Audio::AudioExporter exporter(nullptr);
    show(Audio::Internal::AudioExportProgressDialog(&exporter, &window), Module::Audio,
         QStringLiteral("Export audio - progress"));

    const QList<MidiImportTrackInfo> midiTracks{
        {.name = "Lead",
         .rangeText = QStringLiteral("C4 - E4"),
         .noteCount = 3,
         .lyrics = {"la", "la", "la"},
         .selectedByDefault = true}
    };
    ProjectImportConfigDialog projectImport(&window);
    projectImport.setPage(new MidiConfigPage(midiTracks));
    gallery.show(projectImport, Module::Audio, QStringLiteral("Project import - MIDI options"));
    show(MidiConverterDialog(midiTracks, &window), Module::Audio, QStringLiteral("MIDI converter"));
    show(MidiBatchImportDialog("UTF-8", &window), Module::Audio,
         QStringLiteral("Batch MIDI import"));
    show(ExtractPitchParamDialog({audioClip}), Module::Audio,
         QStringLiteral("Extract pitch - source selection"));
    ResourceCheckDialog resources(&window);
    resources.addPage(new AudioResourcePage({}, {audioClip->id()}));
    gallery.show(resources, Module::Packages, QStringLiteral("Audio resource check"));

    show(EditTempoDialog(480, 120.0, &window), Module::Tracks, QStringLiteral("Edit tempo"));
    show(EditTimeSignatureDialog(1, 4, 4, &window), Module::Tracks,
         QStringLiteral("Edit time signature"));
    show(QuantizeDialog(&window), Module::Notes, QStringLiteral("Quantize notes"));
    show(SearchDialog(singingClip, &window), Module::Lyrics, QStringLiteral("Search lyrics"));
    QList<Note *> notes;
    for (auto *note : singingClip->notes())
        notes.append(note);
    QVERIFY(!notes.isEmpty());
    show(PhonemeEditorDialog(notes.first(), &window), Module::Lyrics,
         QStringLiteral("Edit phonemes"));
    {
        LyricDialog lyrics(singingClip, notes, singingClip->singerIdentifier(),
                           {TestSupport::fixtureLanguage()}, &window);
        lyrics.show();
        lyrics.activateWindow();
        gallery.take(lyrics, Module::Lyrics, QStringLiteral("Fill lyrics"));
        for (auto *button : lyrics.findChildren<QAbstractButton *>()) {
            if (button->text() == QStringLiteral("Lyric Prev") ||
                button->text() == QStringLiteral("Fold Preview")) {
                button->click();
                gallery.take(lyrics, Module::Lyrics,
                             QStringLiteral("Fill lyrics - preview toggled"));
                break;
            }
        }
        auto *tabs = lyrics.findChild<QTabWidget *>();
        QVERIFY(tabs && tabs->count() > 1);
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            gallery.take(lyrics, Module::Lyrics,
                         QStringLiteral("Fill lyrics - %1").arg(tabs->tabText(i)));
        }
    }
    const SingerInfo singer(
        {QStringLiteral("mix-test"), QStringLiteral("gallery"), QVersionNumber(1, 0)},
        QStringLiteral("Demo voice"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm"))});
    show(SpeakerMixDialog(singer, {}, &window), Module::Mixing, QStringLiteral("Speaker mix"));

    MessageDialog message(QStringLiteral("Save changes?"),
                          QStringLiteral("The current project has unsaved changes."), &window);
    message.addAccentButton(QStringLiteral("Save"), 0);
    message.addButton(QStringLiteral("Discard"), 1);
    message.addButton(QStringLiteral("Cancel"), 2);
    gallery.show(message, Module::Workspace, QStringLiteral("Unsaved project confirmation"));
    TaskDialog task(nullptr, true, true, &window);
    task.setTitle(QStringLiteral("Processing project"));
    task.setMessage(QStringLiteral("Preparing audio resources"));
    task.setProgressRange(0, 100);
    task.setProgressValue(45);
    gallery.show(task, Module::Diagnostics, QStringLiteral("Task progress"));
    show(RestartDialog(QStringLiteral("Restart the editor to apply the settings."), true, &window),
         Module::Settings, QStringLiteral("Restart prompt"));
}
