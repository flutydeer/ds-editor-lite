#pragma once

#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include <lite/Support/StringUtils.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace TestSupport {
    inline bool initializeApplicationResources() {
#ifdef LITE_TEST_APPLICATION_PLUGIN_ROOT
        const auto root = QString::fromUtf8(LITE_TEST_APPLICATION_PLUGIN_ROOT);
        if (!QDir::isAbsolutePath(root) || !QDir(root).exists()) {
            qWarning() << "The application runtime plugin directory is unavailable:" << root;
            return false;
        }
        if (!qputenv("DSEL_TEST_PLUGIN_ROOT", root.toUtf8()))
            return false;
#endif

        const QDir configRoot(QString::fromUtf8(LITE_TEST_APPLICATION_CONFIG_ROOT));
        const auto splitterRoot = configRoot.filePath(QStringLiteral("splitter"));
        const auto taggerRoot = configRoot.filePath(QStringLiteral("tagger"));
        for (const auto &path : {splitterRoot, taggerRoot}) {
            if (!QDir::isAbsolutePath(path) || !QFileInfo(path).isDir()) {
                qWarning() << "The application lyric rule directory is unavailable:" << path;
                return false;
            }
        }

        try {
            const auto splitterPath = StringUtils::qstr_to_path(splitterRoot);
            const auto taggerPath = StringUtils::qstr_to_path(taggerRoot);
            if (!FillLyric::TextSplitter::init(splitterPath) ||
                !FillLyric::TextTagger::init(taggerPath, taggerPath) ||
                FillLyric::TextSplitter::builtinNames().isEmpty() ||
                FillLyric::TextTagger::builtinLanguages().isEmpty()) {
                qWarning() << "The application lyric rules could not be loaded:"
                           << configRoot.absolutePath();
                return false;
            }
        } catch (const std::filesystem::filesystem_error &error) {
            qWarning() << "The application lyric rules could not be read:" << error.what();
            return false;
        }
        return true;
    }
}
