#include "tst_application_gui.h"

#include "../TestSupport/MainWindowFixture.h"
#include "../TestSupport/VoicebankFixture.h"
#include "../TestSupport/WaveFixture.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/Utils/ParamUtils.h"
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
#include "UI/Views/ClipEditor/CommonParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/GhostNoteOverlay.h"
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
#include <lite/ProjectModel/InferenceData/InferPiece.h>
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
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QLineEdit>
#include <QMouseEvent>
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
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <exception>

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

    void requireCapture(bool success, const QString &message) {
        if (!success)
            throw std::runtime_error(message.toStdString());
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
            requireCapture(
                m_output.mkpath(QStringLiteral("modules/%1").arg(moduleId(module))),
                QStringLiteral("Screenshot directory, image allocation or summary write failed"));
            // References belong beside the module's other original captures too.
            QFile source(m_output.filePath(name));
            requireCapture(source.open(QIODevice::ReadOnly), source.errorString());
            QSaveFile destination(path);
            requireCapture(destination.open(QIODevice::WriteOnly), destination.errorString());
            const auto bytes = source.readAll();
            requireCapture(source.error() == QFileDevice::NoError, source.errorString());
            requireCapture(destination.write(bytes) == bytes.size(), destination.errorString());
            requireCapture(destination.commit(), destination.errorString());
            addImage(module, title, path, method);
        }

        void take(QWidget &widget, const Module module, const QString &title,
                  const Capture capture = Capture::NativeWindow) {
            // QWidget::grab renders the panel directly, even when its native host
            // is occluded. Only compositor captures require an exposed window.
            if (capture == Capture::NativeWindow)
                requireCapture(QTest::qWaitForWindowExposed(widget.window()),
                               QStringLiteral("Screenshot operation failed: %1").arg(title));
            requireCapture(QTest::qWaitFor([&] { return widget.isVisible(); }),
                           QStringLiteral("Widget was not visible: %1").arg(title));
            // Tab/preview switches can resize a native window. Complete Qt layout
            // and repaint before allowing the compositor to capture its next frame.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            if (widget.layout())
                widget.layout()->activate();
            widget.repaint();
            QTest::qWait(300);
            requireCapture(
                m_output.mkpath(QStringLiteral("modules/%1").arg(moduleId(module))),
                QStringLiteral("Screenshot directory, image allocation or summary write failed"));
            const auto path = imagePath(module, title);
            QString method;
            if (capture == Capture::WidgetContent) {
                // Custom editor panels and popups do not need a repeated host window.
                // Keep exactly their rendered contents, without adding decorative chrome.
                requireCapture(widget.grab().save(path),
                               QStringLiteral("Screenshot operation failed: %1").arg(title));
                method = QStringLiteral("qt-widget-content");
            } else {
                requireCapture(widget.isWindow(),
                               QStringLiteral("Screenshot operation failed: %1").arg(title));
                requireCapture(WindowFrameUtils::captureWindowChrome(&widget, path, &method),
                               QStringLiteral("Screenshot operation failed: %1").arg(title));
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
            requireCapture(!m_entries.isEmpty(), QStringLiteral("No screenshots were requested"));
            QList<Module> modules;
            for (const auto &entry : m_entries) {
                if (!modules.contains(entry.module))
                    modules.append(entry.module);
            }
            std::sort(modules.begin(), modules.end());
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
            requireCapture(
                minimumWidth <= maximumRowWidth,
                QStringLiteral("Screenshot directory, image allocation or summary write failed"));
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
            requireCapture(
                !sheet.isNull(),
                QStringLiteral("Screenshot directory, image allocation or summary write failed"));
            const bool dark = captureTheme() == QStringLiteral("dark");
            sheet.fill(QColor(dark ? "#353f50" : "#c4cdd9"));
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
            requireCapture(
                sheet.save(m_output.filePath(QStringLiteral("ui-review.png"))),
                QStringLiteral("Screenshot directory, image allocation or summary write failed"));
            for (qsizetype i = 0; i < sections.size(); ++i) {
                const auto path = m_output.filePath(
                    QStringLiteral("modules/%1/review.png").arg(moduleId(sections.at(i).module)));
                requireCapture(
                    sheet.copy(sections.at(i).bounds).save(path),
                    QStringLiteral(
                        "Screenshot directory, image allocation or summary write failed"));
            }
            const QJsonObject report{
                {"complete", true                                      },
                {"theme",    captureTheme()                            },
                {"themeId",  ThemeManager::instance()->currentThemeId()},
                {"size",     QJsonArray{sheet.width(), sheet.height()} },
                {"layout",   QStringLiteral("flow")                    },
                {"sections", sectionReports                            }
            };
            QSaveFile manifest(m_output.filePath(QStringLiteral("ui-review.json")));
            requireCapture(manifest.open(QIODevice::WriteOnly), manifest.errorString());
            const auto json = QJsonDocument(report).toJson();
            requireCapture(manifest.write(json) == json.size(), manifest.errorString());
            requireCapture(manifest.commit(), manifest.errorString());
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
            requireCapture(!image.isNull(),
                           QStringLiteral("Screenshot operation failed: %1").arg(path));
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

void ApplicationGuiTests::uiGalleryLayoutGroupsModules() try {
    QTemporaryDir output;
    QVERIFY(output.isValid());

    struct Sample {
        Module module;
        QString title;
        QSize size;
    };

    // Synthetic inputs exercise grouping, wrapping and different aspect ratios;
    // they are not a checklist of production screenshots.
    const QList<Sample> samples{
        {Module::Workspace, QStringLiteral("Wide sample"),    {1500, 700}},
        {Module::Tracks,    QStringLiteral("Another module"), {700, 400} },
        {Module::Workspace, QStringLiteral("Tall sample"),    {600, 1000}},
        {Module::Workspace, QStringLiteral("Small sample"),   {180, 90}  },
        {Module::Workspace, QStringLiteral("Wrap sample A"),  {1500, 700}},
        {Module::Workspace, QStringLiteral("Wrap sample B"),  {1500, 700}},
        {Module::Workspace, QStringLiteral("Wrap sample C"),  {1500, 700}},
    };
    Gallery gallery(output.path());
    for (const auto &sample : samples) {
        const auto path = sample.title + QStringLiteral(".png");
        QImage image(sample.size, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(output.filePath(path)));
        gallery.addReference(sample.module, sample.title, path);
    }
    gallery.writeSheet();
    QFile file(output.filePath("ui-review.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto report = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(report.value("complete").toBool());
    QCOMPARE(report.value("theme").toString(), captureTheme());
    const int sheetWidth = report.value("size").toArray()[0].toInt();
    const int sheetHeight = report.value("size").toArray()[1].toInt();
    QVERIFY(sheetWidth <= 5120);
    const auto rect = [](const QJsonValue &value) {
        const auto a = value.toArray();
        return QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
    };
    QList<QString> remaining;
    for (const auto &sample : samples)
        remaining.append(sample.title);
    int previousBottom = -1;
    int contentRight = 0;
    bool wrapped = false;
    for (const auto &sectionValue : report.value("sections").toArray()) {
        const auto section = sectionValue.toObject();
        const auto heading = rect(section.value("headingRect"));
        QVERIFY(heading.top() > previousBottom);
        const auto images = section.value("images").toArray();
        QVERIFY(!images.isEmpty());
        QList<QRect> occupied;
        for (const auto &imageValue : images) {
            const auto image = imageValue.toObject();
            const auto title = image.value("caption").toString();
            const auto sample = std::find_if(samples.begin(), samples.end(),
                                             [&](const Sample &s) { return s.title == title; });
            QVERIFY(sample != samples.end());
            QVERIFY(remaining.removeOne(title));
            QCOMPARE(section.value("id").toString(), moduleId(sample->module));
            QCOMPARE(section.value("title").toString(), moduleTitle(sample->module));
            const auto path = image.value("file").toString();
            QVERIFY(path.startsWith(QStringLiteral("modules/%1/").arg(moduleId(sample->module))));
            QCOMPARE(QImage(output.filePath(path)).size(), sample->size);
            const auto pixels = rect(image.value("imageRect"));
            const auto caption = rect(image.value("captionRect"));
            const auto expectedSize = sample->size.width() > 1168 || sample->size.height() > 850
                                          ? sample->size.scaled({1168, 850}, Qt::KeepAspectRatio)
                                          : sample->size;
            QCOMPARE(pixels.size(), expectedSize);
            QCOMPARE(caption.width(), qMax(320, expectedSize.width()));
            QVERIFY(pixels.top() > heading.bottom());
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
            const auto bounds = pixels.united(caption);
            contentRight = qMax(contentRight, bounds.right() + 1);
            QVERIFY(QRect(64, 0, sheetWidth - 128, sheetHeight).contains(bounds));
            for (const auto &other : occupied)
                QVERIFY(!bounds.intersects(other));
            occupied.append(bounds);
            previousBottom = qMax(previousBottom, bounds.bottom());
            wrapped |= image.value("row").toInt() > 0;
        }
        QVERIFY(!QImage(output.filePath(section.value("file").toString())).isNull());
    }
    QVERIFY(remaining.isEmpty());
    QVERIFY(wrapped);
    QCOMPARE(sheetWidth, contentRight + 64);
    QCOMPARE(QImage(output.filePath("ui-review.png")).pixelColor(0, 0),
             QColor(captureTheme() == "dark" ? "#353f50" : "#c4cdd9"));
} catch (const std::exception &error) {

    QFAIL(error.what());
}

void ApplicationGuiTests::uiGalleryCaptureFailuresStopImmediately() {
    QTemporaryDir output;
    QVERIFY(output.isValid());
    Gallery gallery(output.path());
    bool continued = false;
    const auto sequence = [&] {
        gallery.addReference(Module::Workspace, QStringLiteral("Missing"), "missing.png");
        continued = true;
        gallery.writeSheet();
    };
    QVERIFY_EXCEPTION_THROWN(sequence(), std::runtime_error);
    QVERIFY(!continued);
    QVERIFY(!QFileInfo::exists(output.filePath("ui-review.json")));

    QImage image(20, 20, QImage::Format_RGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(output.filePath("sample.png")));
    gallery.addReference(Module::Workspace, QStringLiteral("Sample"), "sample.png");
    QVERIFY(QDir(output.path()).mkpath("ui-review.png"));
    QVERIFY_EXCEPTION_THROWN(gallery.writeSheet(), std::runtime_error);
    QVERIFY(!QFileInfo::exists(output.filePath("ui-review.json")));

    QTemporaryDir blocked;
    QVERIFY(blocked.isValid());
    QFile file(blocked.filePath("modules"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    Gallery failedCapture(blocked.path());
    QWidget widget;
    widget.resize(50, 50);
    widget.show();
    QVERIFY_EXCEPTION_THROWN(failedCapture.take(widget, Module::Workspace,
                                                QStringLiteral("Cannot save"),
                                                Gallery::Capture::WidgetContent),
                             std::runtime_error);
    QVERIFY(!QFileInfo::exists(blocked.filePath("ui-review.json")));
}

void ApplicationGuiTests::captureUiGallery() try {
    const auto output = qEnvironmentVariable("LITE_UI_GALLERY");
    if (output.isEmpty())
        QSKIP("Set LITE_UI_GALLERY to generate the UI review artifact");
    QVERIFY(QDir().mkpath(output));
    Gallery gallery(output);
    const auto nativeReference = QStringLiteral("modules/01-workspace/title-bar-native-frame.png");
    const auto framelessReference = QStringLiteral("modules/01-workspace/title-bar-frameless.png");
    if (qEnvironmentVariableIsSet("LITE_SCREENSHOT_DESKTOP_CAPTURE") ||
        QFileInfo::exists(QDir(output).filePath(nativeReference))) {
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
    QVERIFY(runtime.project().setTrackPan(commandContext(), trackId, 0.3));
    QVERIFY(context->m_appModel->tracks().at(0)->control().solo());
    QVERIFY(context->m_appModel->tracks().at(1)->control().mute());

    // The fixture already contains a third track. Give it useful reference
    // notes and resolve every track's color in its final visual order.
    QCOMPARE(context->m_appModel->tracks().size(), 3);
    for (qsizetype index = 0; index < context->m_appModel->tracks().size(); ++index) {
        auto *item = context->m_appModel->tracks().at(index);
        QVERIFY(runtime.project().setTrackColor(commandContext(), Automation::TrackId(item->id()),
                                                static_cast<int>(index)));
    }
    const auto harmonyTrackId = Automation::TrackId(context->m_appModel->tracks().last()->id());
    QVERIFY(runtime.project().renameTrack(commandContext(), harmonyTrackId,
                                          QStringLiteral("Harmony reference")));
    Automation::ClipDraftDto harmony;
    harmony.properties.name = QStringLiteral("Harmony reference");
    harmony.properties.length = harmony.properties.clipLen = 3840;
    harmony.defaultLanguage = QStringLiteral("eng");
    for (int index = 0; index < 4; ++index) {
        Automation::NoteDraftDto note;
        note.localStart = 240 + index * 720;
        note.length = 480;
        note.keyIndex = index % 2 == 0 ? 62 : 64;
        note.lyric = QStringLiteral("la");
        note.language = QStringLiteral("eng");
        harmony.notes.append(note);
    }
    QVERIFY(runtime.project().insertClips(commandContext(),
                                          {
                                              {.trackId = harmonyTrackId, .clip = harmony}
    }));
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto originalAppearance = settings.get().appearance;
    const auto restoreAppearance =
        qScopeGuard([&] { runtime.settings().updateAppearance({}, originalAppearance); });
    auto appearance = originalAppearance;
    appearance.showGhostNotes = true;
    QVERIFY(runtime.settings().updateAppearance({}, appearance));

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
                                                  Param::Edited, {handDrawn, anchors}));
    QCOMPARE(singingClip->params.breathiness.curves(Param::Edited).size(), 2);

    Automation::CurveDraftDto generatedPitch;
    Automation::CurveDraftDto drawnPitch;
    drawnPitch.localStart = 240;
    for (int tick = 0; tick < 3840; tick += generatedPitch.step) {
        const double phase = 2 * std::numbers::pi * tick / 1440.0;
        generatedPitch.values.append(static_cast<int>(6000 + 80 * std::sin(phase)));
        if (tick >= 240 && tick < 1200)
            drawnPitch.values.append(static_cast<int>(6120 + 110 * std::sin(phase * 1.7)));
    }
    Automation::CurveDraftDto pitchAnchors;
    pitchAnchors.type = Automation::CurveDraftDto::Type::Anchor;
    pitchAnchors.nodes = {
        {1440, 5980, AnchorNode::Linear },
        {1980, 6250, AnchorNode::Hermite},
        {2520, 6070, AnchorNode::Hermite},
        {3060, 6400, AnchorNode::None   },
        {3600, 5900, AnchorNode::Linear }
    };
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Pitch,
                                                  Param::Original, {generatedPitch}));
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Pitch,
                                                  Param::Edited, {drawnPitch, pitchAnchors}));
    QCOMPARE(singingClip->params.pitch.curves(Param::Original).size(), 1);
    QCOMPARE(singingClip->params.pitch.curves(Param::Edited).size(), 2);

    // Pitch edits invalidate variance inference. Wait for that work before
    // seeding the generated parameter layer, or inference replaces it with
    // the bundled voicebank's flat defaults while screenshots are being taken.
    QTRY_VERIFY_WITH_TIMEOUT(
        std::all_of(singingClip->pieces().cbegin(), singingClip->pieces().cend(),
                    [](const InferPiece *piece) {
                        return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                               piece->state == QStringLiteral("Ready");
                    }) &&
            taskManager->tasks().isEmpty(),
        15000);
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Breathiness,
                                                  Param::Original, {generated}));
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), clipId, ParamInfo::Tension,
                                                  Param::Original, {background}));
    const auto verifyGeneratedParameters = [&] {
        const auto &curves = singingClip->params.breathiness.curves(Param::Original);
        QCOMPARE(curves.size(), 1);
        const auto *curve = dynamic_cast<const DrawCurve *>(curves.first());
        QVERIFY(curve);
        QCOMPARE(curve->values(), generated.values);
        QVERIFY(*std::max_element(curve->values().cbegin(), curve->values().cend()) -
                    *std::min_element(curve->values().cbegin(), curve->values().cend()) >
                30000);
    };

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
    auto *piano = editor->findChild<PianoRollGraphicsView *>();
    QVERIFY(piano);
    GhostNoteOverlay *ghostNotes = nullptr;
    for (auto *item : piano->scene()->items()) {
        if (auto *ghost = dynamic_cast<GhostNoteOverlay *>(item))
            ghostNotes = ghost;
    }
    QVERIFY(ghostNotes);
    QTRY_VERIFY(ghostNotes->isVisible());
    const auto framePitchCurves = [&] {
        // Settle panel resizing before centering. Otherwise a small desktop can
        // leave the upper pitch anchors outside the resized piano viewport.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QTest::qWait(100);
        if (!window.centerPianoRollAt(1440, 61.5) || !editor->setPitchViewport(61.5, 1.0))
            return false;
        const auto span = piano->topKeyIndex() - piano->bottomKeyIndex();
        if (span < 8.0 && !editor->setPitchViewport(61.5, std::max(0.1, span / 8.0)))
            return false;
        qInfo() << "Gallery pitch viewport:" << piano->bottomKeyIndex() << piano->topKeyIndex();
        return piano->bottomKeyIndex() <= 58.5 && piano->topKeyIndex() >= 64.5;
    };
    QVERIFY(framePitchCurves());
    gallery.take(window, Module::Workspace, QStringLiteral("Editing workspace"));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    QVERIFY(framePitchCurves());
    auto *parameters = editor->findChild<ParamEditorView *>();
    QVERIFY(parameters);
    auto *parameterPrompt = parameters->findChild<QWidget *>("speakerMixEmptyState");
    QVERIFY(parameterPrompt);
    QTRY_VERIFY(!parameterPrompt->isVisible());
    parameters->graphicsView()->setForegroundBaseCurveVisible(true);
    parameters->graphicsView()->setBackgroundBaseCurveVisible(true);
    // Representative tools for Final, Draw and Anchor display styles. Other
    // tools share those static appearances and do not need duplicate pictures.
    const std::pair<EditorViewGlobal::PianoRollEditMode, const char *> pianoStyles[] = {
        {EditorViewGlobal::Select,          "Piano roll - notes"},
        {EditorViewGlobal::DrawPitch,       "Pitch - draw"      },
        {EditorViewGlobal::EditPitchAnchor, "Pitch - anchor"    },
    };
    for (const auto &[mode, caption] : pianoStyles) {
        QVERIFY(editor->setEditMode(mode));
        QVERIFY(editor->setParameterEditMode(mode == EditorViewGlobal::EditPitchAnchor
                                                 ? EditorViewGlobal::ParameterEditMode::Anchor
                                                 : EditorViewGlobal::ParameterEditMode::Draw));
        QCOMPARE(editor->viewState().editMode, mode);
        verifyGeneratedParameters();
        if (QTest::currentTestFailed())
            return;
        gallery.take(*bottom, Module::Notes, QString::fromLatin1(caption),
                     Gallery::Capture::WidgetContent);
    }
    // Shape and Scale share the range/shoulder/percentage overlay. Show one
    // alongside pitch modulation instead of duplicating every armed tool.
    QVERIFY(editor->setEditMode(EditorViewGlobal::ModulatePitch));
    QVERIFY(editor->setParameterEditMode(EditorViewGlobal::ParameterEditMode::Scale));
    const auto selectTransformRange = [&](TimeGraphicsView &graphics, double value) {
        CommonParamEditorView *foreground = nullptr;
        for (auto *item : graphics.scene()->items()) {
            auto *candidate = dynamic_cast<CommonParamEditorView *>(item);
            if (candidate && !candidate->transparentMouseEvents()) {
                foreground = candidate;
                break;
            }
        }
        QVERIFY(foreground);
        const auto point = [&](int tick) {
            const auto visible = graphics.visibleRect();
            const auto ratio =
                (tick - graphics.startTick()) / (graphics.endTick() - graphics.startTick());
            return graphics.mapFromScene(QPointF(visible.left() + ratio * visible.width(),
                                                 foreground->sceneYForValue(value)));
        };
        const auto start = point(480);
        const auto end = point(1080);
        QVERIFY(graphics.viewport()->rect().contains(start));
        QVERIFY(graphics.viewport()->rect().contains(end));
        const auto before = graphics.viewport()->grab().toImage();
        QTest::mousePress(graphics.viewport(), Qt::LeftButton, Qt::NoModifier, start);
        QMouseEvent move(QEvent::MouseMove, QPointF(end),
                         QPointF(graphics.viewport()->mapToGlobal(end)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(graphics.viewport(), &move);
        QTest::mouseRelease(graphics.viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QCoreApplication::processEvents();
        QVERIFY(before != graphics.viewport()->grab().toImage());
    };
    selectTransformRange(*piano, 6000);
    if (QTest::currentTestFailed())
        return;
    selectTransformRange(*parameters->graphicsView(), -40000);
    if (QTest::currentTestFailed())
        return;
    gallery.take(*bottom, Module::Notes, QStringLiteral("Pitch and parameter transform ranges"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(editor->setEditMode(EditorViewGlobal::Select));
    QVERIFY(editor->setParameterEditMode(EditorViewGlobal::ParameterEditMode::Anchor));
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
    QVERIFY(framePitchCurves());
    gallery.take(*detached, Module::Notes, QStringLiteral("Detached piano roll and parameters"));
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
    std::exception_ptr settingsCaptureFailure;
    QTimer::singleShot(0, &window, [&] {
        try {
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
        } catch (...) {
            // Never let a capture exception escape through Qt's event dispatcher.
            settingsCaptureFailure = std::current_exception();
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                dialog->reject();
        }
    });
    window.openAppOptions(AppOptionsGlobal::General);
    if (settingsCaptureFailure)
        std::rethrow_exception(settingsCaptureFailure);
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
    SingerInfo singer({QStringLiteral("mix-test"), QStringLiteral("gallery"), QVersionNumber(1, 0)},
                      QStringLiteral("Demo voice"),
                      {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
                       SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm"))});
    SingerCapabilitySummary capability;
    capability.acousticParameters =
        QStringList{QStringLiteral("breathiness"), QStringLiteral("tension")};
    singer.setCapability(capability);
    // Use the same two-speaker data as the dynamic-mix interaction tests rather
    // than capturing an unavailable-state placeholder under an editor caption.
    const SpeakerInfo bright(QStringLiteral("bright"), QStringLiteral("Bright"));
    const SpeakerInfo warm(QStringLiteral("warm"), QStringLiteral("Warm"));
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    mix.sources = {{bright}, {warm}};
    mix.fixedWeights = {0.5};
    QVERIFY(runtime.parameters().applyClipSpeakerMix(
        commandContext(), Automation::ClipId(singingClip->id()), singer, bright, mix));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QVERIFY(window.setEditorPanelVisibility(false, true));
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(!paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, singingClip->singerInfo()));
    QVERIFY(editor->setParameterForeground(ParamInfo::MouthOpening));
    QTRY_VERIFY(parameterPrompt->isVisible());
    gallery.take(*parameters, Module::Notes, QStringLiteral("Unsupported parameter"),
                 Gallery::Capture::WidgetContent);
    QVERIFY(editor->setParameterForeground(ParamInfo::SpeakerMix));
    auto *mixEmpty = editor->findChild<QWidget *>("speakerMixEmptyState");
    QVERIFY(mixEmpty);
    QTRY_VERIFY(mixEmpty->isVisible());
    auto *mixEditor = editor->findChild<ParamEditorView *>();
    QVERIFY(mixEditor);
    QTRY_VERIFY(mixEditor->isVisible());
    gallery.take(*mixEditor, Module::Mixing, QStringLiteral("Enable dynamic speaker mix"),
                 Gallery::Capture::WidgetContent);
    auto *enableDynamicMix = mixEmpty->findChild<QAbstractButton *>();
    QVERIFY(enableDynamicMix && enableDynamicMix->isVisible() && enableDynamicMix->isEnabled());
    QTest::mouseClick(enableDynamicMix, Qt::LeftButton);
    QTRY_VERIFY(!mixEmpty->isVisible());
    QCOMPARE(singingClip->speakerMixData().mode, SpeakerMixModel::SingerSourceMode::DynamicMix);
    mix = singingClip->speakerMixData();
    mix.dynamicKeyframes = {
        {0,   {0.5} },
        {480, {0.25}},
        {960, {0.75}}
    };
    QVERIFY(
        runtime.parameters().applyClipSpeakerMix(commandContext(), clipId, singer, bright, mix));
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
    gallery.writeSheet();
} catch (const std::exception &error) {

    QFAIL(error.what());
}
