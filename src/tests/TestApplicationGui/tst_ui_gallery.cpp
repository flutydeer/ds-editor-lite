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
#include "Modules/FillLyric/Controls/PhonicTextEdit.h"
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
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/MainTitleBar/FilePopupWidget.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Window/DetachedPanelWindow.h"
#include "UI/Window/LogWindow.h"
#include "UI/Window/TaskWindow.h"

#include <lite/GUI/Utils/WindowFrameUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/Task.h>
#include <lite/Tasking/TaskManager.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QImage>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPainter>
#include <QScopeGuard>
#include <QTabWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QLayout>
#include <QRegularExpression>
#include <QStringList>

#include <numbers>

namespace {
    // Keep the real task widget at a reproducible progress point during capture.
    class GalleryTask final : public Task {
    protected:
        void runTask() override {
            while (!isTerminateRequested())
                QThread::msleep(20);
        }
    };

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

    QString moduleId(const Module module) {
        static const QStringList ids{"01-workspace", "02-tracks",   "03-notes",
                                     "04-lyrics",    "05-mixing",   "06-audio",
                                     "07-packages",  "08-settings", "09-diagnostics"};
        return ids.at(static_cast<int>(module));
    }

    QString captureTheme() {
        return qEnvironmentVariable("LITE_CAPTURE_THEME", "light");
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
            const QFileInfo sourceInfo(m_output.filePath(name));
            QFile report(sourceInfo.dir().filePath(sourceInfo.completeBaseName() + ".json"));
            QString method = QStringLiteral("reference-image");
            if (report.open(QIODevice::ReadOnly)) {
                method = QJsonDocument::fromJson(report.readAll())
                             .object()
                             .value(QStringLiteral("captureMethod"))
                             .toString(method);
            }
            const auto moduleDirectory = QStringLiteral("modules/%1").arg(moduleId(module));
            if (QDir::cleanPath(sourceInfo.absolutePath()) ==
                QDir::cleanPath(QDir(m_output.filePath(moduleDirectory)).absolutePath())) {
                addImage(module, title, sourceInfo.filePath(), method);
                return;
            }
            const auto path = imagePath(module, title);
            QVERIFY(m_output.mkpath(QStringLiteral("modules/%1").arg(moduleId(module))));
            // References belong beside the module's other original captures too.
            QFile source(m_output.filePath(name));
            QVERIFY(source.open(QIODevice::ReadOnly));
            QFile destination(path);
            QVERIFY(destination.open(QIODevice::WriteOnly));
            const auto bytes = source.readAll();
            QCOMPARE(destination.write(bytes), bytes.size());
            destination.close();
            addImage(module, title, path, method);
        }

        void take(QWidget &widget, const Module module, const QString &title,
                  const Capture capture = Capture::NativeWindow) {
            // QWidget::grab renders the panel directly, even when its native host
            // is occluded. Only compositor captures require an exposed window.
            if (capture == Capture::NativeWindow)
                QVERIFY2(QTest::qWaitForWindowExposed(widget.window()), qPrintable(title));
            QTRY_VERIFY(widget.isVisible());
            // Tab/preview switches can resize a native window. Complete Qt layout
            // and repaint before allowing the compositor to capture its next frame.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            if (widget.layout())
                widget.layout()->activate();
            widget.repaint();
            QTest::qWait(300);
            QVERIFY(m_output.mkpath(QStringLiteral("modules/%1").arg(moduleId(module))));
            const auto path = imagePath(module, title);
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
            constexpr int margin = 64;
            constexpr int columnGap = 48;
            constexpr int rowGap = 44;
            constexpr int sectionGap = 64;
            constexpr int headingHeight = 52;
            constexpr int headingGap = 24;
            constexpr int captionGap = 12;
            constexpr int minimumCaptionWidth = 320;
            QFont headingFont = QApplication::font();
            headingFont.setPixelSize(40);
            headingFont.setBold(true);
            QFont captionFont = headingFont;
            captionFont.setPixelSize(22);
            captionFont.setBold(false);
            const QFontMetrics captionMetrics(captionFont);

            struct Placement {
                const Entry *entry;
                QRect image;
                QRect caption;
                int row;
            };

            struct Section {
                Module module;
                QRect heading;
                QRect bounds;
                QList<Placement> images;
            };

            // Measure every module at a candidate width using the same flow as painting.
            // Narrow dialogs consume only their own width (or a readable caption width).
            const auto layout = [&](const int width, QList<Section> *result = nullptr) {
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
                        module, QRect(margin, y, width - 2 * margin, headingHeight), {}, {}};
                    y += headingHeight + headingGap;
                    int x = margin;
                    int rowImageHeight = 0;
                    int row = 0;
                    qsizetype rowStart = 0;
                    const auto finishRow = [&] {
                        int bottom = y;
                        for (qsizetype i = rowStart; i < section.images.size(); ++i) {
                            auto &placement = section.images[i];
                            placement.image.moveTop(
                                y + (rowImageHeight - placement.image.height()) / 2);
                            placement.caption.moveTop(placement.image.bottom() + 1 + captionGap);
                            bottom = qMax(bottom, placement.caption.bottom() + 1);
                        }
                        y = bottom;
                    };
                    for (const auto *entry : entries) {
                        const int itemWidth = qMax(minimumCaptionWidth, entry->image.width());
                        if (x > margin && x + itemWidth > width - margin) {
                            finishRow();
                            y += rowGap;
                            x = margin;
                            rowImageHeight = 0;
                            ++row;
                            rowStart = section.images.size();
                        }
                        const int captionHeight =
                            captionMetrics
                                .boundingRect(QRect(0, 0, itemWidth, 10000),
                                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                                              entry->title)
                                .height();
                        const int imageX = x + (itemWidth - entry->image.width()) / 2;
                        section.images.append({entry, QRect(QPoint(imageX, y), entry->image.size()),
                                               QRect(x, y + entry->image.height() + captionGap,
                                                     itemWidth, captionHeight),
                                               row});
                        rowImageHeight = qMax(rowImageHeight, entry->image.height());
                        x += itemWidth + columnGap;
                    }
                    finishRow();
                    section.bounds = QRect(0, sectionTop - 24, width, y - sectionTop + 48);
                    if (result)
                        result->append(std::move(section));
                    y += sectionGap;
                }
                return y - sectionGap + margin;
            };
            int minimumWidth = minimumCaptionWidth + 2 * margin;
            for (const auto &entry : m_entries)
                minimumWidth = qMax(minimumWidth, entry.image.width() + 2 * margin);
            const QFontMetrics headingMetrics(headingFont);
            for (const auto module : modules)
                minimumWidth =
                    qMax(minimumWidth,
                         headingMetrics.horizontalAdvance(moduleTitle(module)) + 2 * margin);
            // Each module wraps independently at this maximum, with no aspect
            // target or column count. Stacking more modules only adds height.
            constexpr int maximumRowWidth = 5120;
            QVERIFY(minimumWidth <= maximumRowWidth);
            const auto contentWidth = [&](const QList<Section> &sections) {
                int right = minimumWidth - margin;
                for (const auto &section : sections) {
                    for (const auto &placement : section.images)
                        right = qMax(right, placement.caption.right() + 1);
                }
                return right + margin;
            };
            QList<Section> sections;
            const int sheetHeight = layout(maximumRowWidth, &sections);
            const int sheetWidth = contentWidth(sections);
            for (auto &section : sections) {
                section.heading.setWidth(sheetWidth - 2 * margin);
                section.bounds.setWidth(sheetWidth);
            }
            QImage sheet(sheetWidth, sheetHeight, QImage::Format_RGB32);
            QVERIFY(!sheet.isNull());
            const bool dark = captureTheme() == QStringLiteral("dark");
            sheet.fill(QColor(dark ? "#353f50" : "#d7dee8"));
            QPainter painter(&sheet);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.setPen(QColor(dark ? "#e6eaf0" : "#182433"));
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
                        {"captionRect",   rectJson(placement.caption)                   },
                        {"row",           placement.row                                 }
                    });
                }
                sectionReports.append(QJsonObject{
                    {"id",          moduleId(section.module)                                             },
                    {"title",       moduleTitle(section.module)                                          },
                    {"file",        QStringLiteral("modules/%1/review.png").arg(moduleId(section.module))},
                    {"headingRect", rectJson(section.heading)                                            },
                    {"images",      imageReports                                                         }
                });
            }
            painter.end();
            QVERIFY(sheet.save(m_output.filePath(QStringLiteral("ui-review.png"))));
            for (qsizetype i = 0; i < sections.size(); ++i) {
                const auto path = m_output.filePath(
                    QStringLiteral("modules/%1/review.png").arg(moduleId(sections.at(i).module)));
                QVERIFY(sheet.copy(sections.at(i).bounds).save(path));
            }
            const QJsonObject report{
                {"complete", !QTest::currentTestFailed()               },
                {"theme",    captureTheme()                            },
                {"themeId",  ThemeManager::instance()->currentThemeId()},
                {"size",     QJsonArray{sheet.width(), sheet.height()} },
                {"layout",   QStringLiteral("flow")                    },
                {"sections", sectionReports                            }
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
        QString imagePath(const Module module, const QString &title) const {
            auto slug = title.toLower();
            slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), "-");
            slug.remove(QRegularExpression(QStringLiteral("^-|-$")));
            int number = 1;
            for (const auto &entry : m_entries) {
                if (entry.module == module)
                    ++number;
            }
            return m_output.filePath(QStringLiteral("modules/%1/%2-%3.png")
                                         .arg(moduleId(module))
                                         .arg(number, 2, 10, QLatin1Char('0'))
                                         .arg(slug));
        }

        void addImage(const Module module, const QString &title, const QString &path,
                      const QString &captureMethod) {
            QImage image(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            const auto sourceSize = image.size();
            // Preserve readable panel sizes without stretching small dialogs.
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
    QImage small(180, 90, QImage::Format_RGB32);
    small.fill(Qt::blue);
    QVERIFY(small.save(output.filePath("small.png")));
    QVERIFY(QDir(output.path()).mkpath("modules/02-tracks"));
    const auto moduleReference = QStringLiteral("modules/02-tracks/existing.png");
    QVERIFY(wide.save(output.filePath(moduleReference)));
    QFile provenance(output.filePath("modules/02-tracks/existing.json"));
    QVERIFY(provenance.open(QIODevice::WriteOnly));
    const QByteArray provenanceJson = R"({"captureMethod":"qt-client-fallback"})";
    QCOMPARE(provenance.write(provenanceJson), provenanceJson.size());
    provenance.close();
    Gallery gallery(output.path());
    gallery.addReference(Module::Workspace, QStringLiteral("Wide workspace"), "wide.png");
    gallery.addReference(Module::Tracks, QStringLiteral("Track editor"), moduleReference);
    gallery.addReference(Module::Workspace, QStringLiteral("Portrait workspace"), "tall.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Small workspace"), "small.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Third workspace"), "wide.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Fourth workspace"), "wide.png");
    gallery.addReference(Module::Workspace, QStringLiteral("Fifth workspace"), "wide.png");
    gallery.writeSheet();
    if (QTest::currentTestFailed())
        return;
    QFile file(output.filePath("ui-review.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto report = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(report.value("complete").toBool());
    QCOMPARE(report.value("theme").toString(), captureTheme());
    QCOMPARE(report.value("layout").toString(), QStringLiteral("flow"));
    QVERIFY(!report.contains("columns"));
    const int sheetWidth = report.value("size").toArray()[0].toInt();
    const int sheetHeight = report.value("size").toArray()[1].toInt();
    QVERIFY(sheetWidth <= 5120);
    const auto sections = report.value("sections").toArray();
    QCOMPARE(sections.size(), 2);
    QCOMPARE(sections[0].toObject().value("title").toString(), moduleTitle(Module::Workspace));
    QCOMPARE(sections[1].toObject().value("title").toString(), moduleTitle(Module::Tracks));
    QCOMPARE(sections[0].toObject().value("id").toString(), moduleId(Module::Workspace));
    const auto trackReference = sections[1].toObject().value("images").toArray()[0].toObject();
    QCOMPARE(trackReference.value("file").toString(), moduleReference);
    QCOMPARE(trackReference.value("captureMethod").toString(),
             QStringLiteral("qt-client-fallback"));
    const auto rect = [](const QJsonValue &value) {
        const auto a = value.toArray();
        return QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
    };
    int previousBottom = -1;
    int contentRight = 0;
    for (const auto &sectionValue : sections) {
        const auto section = sectionValue.toObject();
        const auto heading = rect(section.value("headingRect"));
        QVERIFY(heading.top() > previousBottom);
        QCOMPARE(heading.left(), 64);
        QCOMPARE(heading.width(), sheetWidth - 128);
        const auto images = section.value("images").toArray();
        QList<QRect> occupied;
        for (const auto &imageValue : images) {
            const auto image = imageValue.toObject();
            const auto path = image.value("file").toString();
            QVERIFY(
                path.startsWith(QStringLiteral("modules/%1/").arg(section.value("id").toString())));
            QVERIFY(!QImage(output.filePath(path)).isNull());
            const auto pixels = rect(image.value("imageRect"));
            const auto caption = rect(image.value("captionRect"));
            QVERIFY(pixels.top() > heading.bottom());
            QVERIFY(caption.top() > pixels.bottom());
            int rowTop = pixels.top();
            int rowImageHeight = pixels.height();
            for (const auto &peerValue : images) {
                const auto peer = peerValue.toObject();
                if (peer.value("row").toInt() != image.value("row").toInt())
                    continue;
                const auto peerPixels = rect(peer.value("imageRect"));
                rowTop = qMin(rowTop, peerPixels.top());
                rowImageHeight = qMax(rowImageHeight, peerPixels.height());
            }
            QCOMPARE(pixels.top(), rowTop + (rowImageHeight - pixels.height()) / 2);
            QCOMPARE(caption.top(), pixels.bottom() + 1 + 12);
            QVERIFY(pixels.width() <= 1168);
            QVERIFY(pixels.height() <= 850);
            const auto bounds = pixels.united(caption);
            contentRight = qMax(contentRight, bounds.right() + 1);
            QVERIFY(QRect(64, 0, sheetWidth - 128, sheetHeight).contains(bounds));
            for (const auto &other : occupied)
                QVERIFY(!bounds.intersects(other));
            occupied.append(bounds);
            previousBottom = qMax(previousBottom, bounds.bottom());
        }
    }
    QCOMPARE(sheetWidth, contentRight + 64);
    const auto workspace = sections[0].toObject().value("images").toArray();
    QCOMPARE(workspace.size(), 6);
    bool wrapped = false;
    for (int i = 1; i < workspace.size(); ++i) {
        const auto previous = rect(workspace[i - 1].toObject().value("captionRect"));
        const auto current = rect(workspace[i].toObject().value("captionRect"));
        if (workspace[i].toObject().value("row") == workspace[i - 1].toObject().value("row")) {
            QCOMPARE(current.left(), previous.right() + 1 + 48);
        } else {
            wrapped = true;
            QCOMPARE(current.left(), 64);
            QVERIFY(previous.right() + 1 + 48 + current.width() > sheetWidth - 64);
        }
    }
    QVERIFY(wrapped);
    QCOMPARE(rect(workspace[0].toObject().value("imageRect")).width(), 1168);
    QCOMPARE(rect(workspace[1].toObject().value("imageRect")).height(), 850);
    QCOMPARE(rect(workspace[1].toObject().value("captionRect")).width(), 510);
    QCOMPARE(rect(workspace[2].toObject().value("imageRect")).size(), QSize(180, 90));
    QCOMPARE(rect(workspace[2].toObject().value("captionRect")).width(), 320);
    QVERIFY(!QImage(output.filePath("ui-review.png")).isNull());
    QVERIFY(!QImage(output.filePath("modules/01-workspace/review.png")).isNull());
    QCOMPARE(QImage(output.filePath("ui-review.png")).pixelColor(0, 0),
             QColor(captureTheme() == "dark" ? "#353f50" : "#d7dee8"));
}

void ApplicationGuiTests::captureUiGallery() {
    const auto output = qEnvironmentVariable("LITE_UI_GALLERY");
    if (output.isEmpty())
        QSKIP("Set LITE_UI_GALLERY to generate the UI review artifact");
    QVERIFY(QDir().mkpath(output));
    Gallery gallery(output);
    const auto compose = qScopeGuard([&] { gallery.writeSheet(); });
    const auto nativeReference = QStringLiteral("modules/01-workspace/title-bar-native-frame.png");
    const auto framelessReference = QStringLiteral("modules/01-workspace/title-bar-frameless.png");
    if (QFileInfo::exists(QDir(output).filePath(nativeReference))) {
        gallery.addReference(Module::Workspace, QStringLiteral("Main window - system frame"),
                             nativeReference);
        gallery.addReference(Module::Workspace, QStringLiteral("Main window - frameless"),
                             framelessReference);
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

    const auto audioTrackId = Automation::TrackId(track.get().affectedObjects.first().value);
    QVERIFY(runtime.project().setTrackSolo(commandContext(), trackId, true));
    QVERIFY(runtime.project().setTrackMute(commandContext(), audioTrackId, true));
    QVERIFY(runtime.project().setTrackGain(commandContext(), audioTrackId, -6.0));
    QVERIFY(runtime.project().setTrackPan(commandContext(), audioTrackId, -0.3));
    QVERIFY(context->m_appModel->tracks().at(0)->control().solo());
    QVERIFY(context->m_appModel->tracks().at(1)->control().mute());

    // Seed the same model layers used by generated, hand-drawn and anchor data.
    // These deterministic fixture values do not require a live inference service.
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    Automation::CurveDraftDto generated;
    Automation::CurveDraftDto handDrawn;
    Automation::CurveDraftDto background;
    generated.localStart = background.localStart = 0;
    handDrawn.localStart = 240;
    for (int tick = 0; tick < 3840; tick += generated.step) {
        const double phase = 2 * std::numbers::pi * tick / 1440.0;
        generated.values.append(static_cast<int>(-36000 + 18000 * std::sin(phase)));
        background.values.append(static_cast<int>(2500 * std::cos(phase * 0.7)));
        if (tick >= 240 && tick < 1200)
            handDrawn.values.append(static_cast<int>(-20000 + 10000 * std::sin(phase * 1.7)));
    }
    Automation::CurveDraftDto anchors;
    anchors.type = Automation::CurveDraftDto::Type::Anchor;
    anchors.nodes = {
        {1440, -52000, AnchorNode::Linear },
        {1980, -14000, AnchorNode::Hermite},
        {2520, -44000, AnchorNode::Hermite},
        {3060, -18000, AnchorNode::None   },
        {3600, -60000, AnchorNode::Linear }
    };
    const auto clipId = Automation::ClipId(singingClip->id());
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Breathiness,
                                                  Param::Original, {generated}));
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Breathiness,
                                                  Param::Edited, {handDrawn, anchors}));
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Tension,
                                                  Param::Original, {background}));
    QCOMPARE(singingClip->params.breathiness.curves(Param::Original).size(), 1);
    QCOMPARE(singingClip->params.breathiness.curves(Param::Edited).size(), 2);

    QVERIFY(window.setEditorPanelVisibility(true, false));
    for (const auto *name : {"actionTempoLane", "actionTimeSignatureLane"}) {
        auto *action = window.findChild<QAction *>(name);
        QVERIFY(action);
        if (action->isChecked())
            action->trigger();
        QVERIFY(!action->isChecked());
    }
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
    QVERIFY(editor->setParameterForeground(ParamInfo::Breathiness));
    QVERIFY(editor->setParameterBackground(ParamInfo::Tension));
    QVERIFY(window.centerPianoRollAt(1440, 60));
    gallery.take(window, Module::Workspace, QStringLiteral("Editing workspace"));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    gallery.take(*bottom, Module::Notes, QStringLiteral("Piano roll and parameters"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(editor->setRegionVisibility(false, true));
    QVERIFY(editor->setParameterEditMode(EditorViewGlobal::ParameterEditMode::Anchor));
    auto *parameters = editor->findChild<ParamEditorView *>();
    QVERIFY(parameters);
    auto *parameterPrompt = parameters->findChild<QWidget *>("speakerMixEmptyState");
    QVERIFY(parameterPrompt);
    QTRY_VERIFY(!parameterPrompt->isVisible());
    parameters->graphicsView()->setForegroundBaseCurveVisible(true);
    parameters->graphicsView()->setBackgroundBaseCurveVisible(true);
    gallery.take(*parameters, Module::Notes, QStringLiteral("Parameter curves"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(editor->setRegionVisibility(true, true));
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
        auto *query = search.findChild<QLineEdit *>();
        auto *results = search.findChild<QListWidget *>();
        QVERIFY(query && results);
        query->setText(TestSupport::fixtureLyric());
        QTRY_VERIFY(results->count() > 0);
        gallery.show(search, Module::Lyrics, QStringLiteral("Search lyrics in detached editor"));
    }
    detached->close();
    QVERIFY(!detached->isVisible());
    QVERIFY(window.setEditorPanelVisibility(true, true));

    LogWindow log(&window);
    gallery.show(log, Module::Diagnostics, QStringLiteral("Log viewer"));
    {
        GalleryTask rendering;
        TaskStatus status;
        status.id = rendering.id();
        status.title = QStringLiteral("Render guide audio");
        status.message = QStringLiteral("Rendering audio resources");
        status.progress = 45;
        rendering.setStatus(status);
        taskManager->addAndStartTask(&rendering);
        const auto finish = qScopeGuard([&] {
            rendering.terminate();
            taskManager->wait();
            QCoreApplication::sendPostedEvents(&rendering, QEvent::MetaCall);
            taskManager->removeTask(&rendering);
        });
        QTRY_VERIFY(rendering.started());
        TaskWindow tasks(&window);
        auto *list = tasks.findChild<QListWidget *>("taskList");
        QVERIFY(list && list->count() > 0);
        gallery.show(tasks, Module::Diagnostics, QStringLiteral("Task list"));
    }

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
    Audio::Internal::AudioExportProgressDialog exportProgress(&exporter, &window);
    emit exporter.progressChanged(0.45, -1);
    gallery.show(exportProgress, Module::Audio, QStringLiteral("Export audio - progress"));

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
    resources.finalizePages();
    resources.resize(1100, 600);
    gallery.show(resources, Module::Packages, QStringLiteral("Audio resource check"));

    show(EditTempoDialog(480, 120.0, &window), Module::Tracks, QStringLiteral("Edit tempo"));
    show(EditTimeSignatureDialog(1, 4, 4, &window), Module::Tracks,
         QStringLiteral("Edit time signature"));
    show(QuantizeDialog(&window), Module::Notes, QStringLiteral("Quantize notes"));
    {
        SearchDialog search(singingClip, &window);
        auto *query = search.findChild<QLineEdit *>();
        auto *results = search.findChild<QListWidget *>();
        QVERIFY(query && results);
        query->setText(TestSupport::fixtureLyric());
        QTRY_VERIFY(results->count() > 0);
        gallery.show(search, Module::Lyrics, QStringLiteral("Search lyrics"));
    }
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
        auto *input = lyrics.findChild<FillLyric::PhonicTextEdit *>();
        QVERIFY(input);
        input->setPlainText(QStringLiteral("la la la"));
        lyrics.resize(960, 640);
        gallery.take(lyrics, Module::Lyrics, QStringLiteral("Fill lyrics"));
        bool previewToggled = false;
        for (auto *button : lyrics.findChildren<QAbstractButton *>()) {
            if (button->text() == QStringLiteral("Lyric Prev") ||
                button->text() == QStringLiteral("Fold Preview")) {
                button->click();
                lyrics.resize(960, 640);
                gallery.take(lyrics, Module::Lyrics, QStringLiteral("Fill lyrics - preview"));
                previewToggled = true;
                break;
            }
        }
        QVERIFY(previewToggled);
        auto *tabs = lyrics.findChild<QTabWidget *>();
        QVERIFY(tabs && tabs->count() > 1);
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            if (auto *testInput =
                    tabs->currentWidget()->findChild<QPlainTextEdit *>("testInputEdit")) {
                testInput->setPlainText(QStringLiteral("la la la"));
                bool tested = false;
                for (auto *button : tabs->currentWidget()->findChildren<QAbstractButton *>()) {
                    if (button->text() == QStringLiteral("Run Test")) {
                        button->click();
                        tested = true;
                        break;
                    }
                }
                QVERIFY(tested);
                auto *result = tabs->currentWidget()->findChild<QPlainTextEdit *>("testOutputEdit");
                QVERIFY(result && !result->toPlainText().isEmpty());
            }
            lyrics.resize(960, 640);
            gallery.take(lyrics, Module::Lyrics,
                         QStringLiteral("Fill lyrics - %1").arg(tabs->tabText(i)));
        }
    }
    const SingerInfo singer(
        {QStringLiteral("mix-test"), QStringLiteral("gallery"), QVersionNumber(1, 0)},
        QStringLiteral("Demo voice"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm"))});
    // Use the same two-speaker data as the dynamic-mix interaction tests rather
    // than capturing an unavailable-state placeholder under an editor caption.
    const SpeakerInfo bright(QStringLiteral("bright"), QStringLiteral("Bright"));
    const SpeakerInfo warm(QStringLiteral("warm"), QStringLiteral("Warm"));
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
    mix.sources = {{bright}, {warm}};
    mix.fixedWeights = {0.5};
    mix.dynamicKeyframes = {
        {0,   {0.5} },
        {480, {0.25}},
        {960, {0.75}}
    };
    QVERIFY(runtime.parameters().applyClipSpeakerMix(
        commandContext(), Automation::ClipId(singingClip->id()), singer, bright, mix));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(editor->setParameterForeground(ParamInfo::SpeakerMix));
    auto *mixEmpty = editor->findChild<QWidget *>("speakerMixEmptyState");
    QVERIFY(mixEmpty);
    QTRY_VERIFY(!mixEmpty->isVisible());
    auto *mixEditor = editor->findChild<ParamEditorView *>();
    QVERIFY(mixEditor);
    QTRY_VERIFY(mixEditor->isVisible());
    gallery.take(*mixEditor, Module::Mixing, QStringLiteral("Dynamic speaker mix"),
                 Gallery::Capture::WidgetContent);
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
