#include "tst_gui_components.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QtTest/QTest>

// Qt resolves every `qproperty-*` declaration against the meta object of each
// widget it polishes. A selector list such as `A, B { qproperty-x: ... }` is
// only valid when both A and B declare x; otherwise Qt prints a warning on
// every polish. This test keeps the bundled themes honest without having to
// instantiate every themed widget.

namespace {

    struct ClassDeclaration {
        QStringList bases;
        QSet<QString> properties;
    };

    using ClassMap = QHash<QString, ClassDeclaration>;

    struct QssRule {
        QString selector;
        QStringList properties;
    };

    QString readTextFile(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    ClassMap collectClassDeclarations(const QString &sourceRoot) {
        static const QRegularExpression classRe(QStringLiteral(
            R"re(\bclass\s+(?:[A-Z_0-9]+\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::\s*([^{;]+))?\{)re"));
        static const QRegularExpression accessRe(
            QStringLiteral(R"re(\b(public|protected|private|virtual)\b)re"));
        static const QRegularExpression templateRe(QStringLiteral(R"re(<[^>]*>)re"));
        static const QRegularExpression identifierRe(
            QStringLiteral(R"re(^[A-Za-z_][A-Za-z0-9_]*$)re"));

        ClassMap classes;
        QDirIterator it(sourceRoot, {QStringLiteral("*.h"), QStringLiteral("*.hpp")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto text = readTextFile(it.next());
            if (text.isEmpty())
                continue;

            QList<QPair<qsizetype, QString>> classStarts;
            auto matches = classRe.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                auto &entry = classes[match.captured(1)];
                QStringList bases;
                for (const auto &part : match.captured(2).split(QLatin1Char(','))) {
                    auto base = part;
                    base.remove(accessRe);
                    base.remove(templateRe);
                    base = base.trimmed().split(QStringLiteral("::")).last().trimmed();
                    if (identifierRe.match(base).hasMatch())
                        bases.append(base);
                }
                entry.bases = bases;
                classStarts.append({match.capturedStart(), match.captured(1)});
            }

            // Q_PROPERTY(type name READ ...) may span lines, so match the
            // parentheses instead of relying on a single-line regex.
            const auto marker = QStringLiteral("Q_PROPERTY(");
            qsizetype pos = 0;
            while ((pos = text.indexOf(marker, pos)) != -1) {
                auto end = pos + marker.size();
                auto depth = 1;
                while (end < text.size() && depth > 0) {
                    const auto ch = text.at(end);
                    if (ch == u'(')
                        ++depth;
                    else if (ch == u')')
                        --depth;
                    ++end;
                }
                const auto body =
                    text.mid(pos + marker.size(), end - pos - marker.size() - 1).simplified();
                const auto tokens = body.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (tokens.size() >= 2) {
                    for (auto i = classStarts.size() - 1; i >= 0; --i) {
                        if (classStarts.at(i).first < pos) {
                            classes[classStarts.at(i).second].properties.insert(tokens.at(1));
                            break;
                        }
                    }
                }
                pos = end;
            }
        }
        return classes;
    }

    QSet<QString> resolvedProperties(const ClassMap &classes, const QString &name,
                                     QSet<QString> &visited) {
        if (visited.contains(name))
            return {};
        visited.insert(name);
        const auto it = classes.constFind(name);
        if (it == classes.constEnd())
            return {};
        auto result = it->properties;
        for (const auto &base : it->bases)
            result.unite(resolvedProperties(classes, base, visited));
        return result;
    }

    QList<QssRule> collectQssRules(QString text) {
        static const QRegularExpression commentRe(QStringLiteral(R"re(/\*.*?\*/)re"),
                                                  QRegularExpression::DotMatchesEverythingOption);
        // Semantic colour placeholders contain braces, which would confuse the
        // rule scanner below. Their values are irrelevant to this check.
        static const QRegularExpression tokenRe(QStringLiteral(R"re(\$\{[^{}]*\})re"));
        static const QRegularExpression ruleRe(QStringLiteral(R"re(([^{}]+)\{([^{}]*)\})re"),
                                               QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression propertyRe(
            QStringLiteral(R"re(qproperty-([A-Za-z_][A-Za-z0-9_]*)\s*:)re"),
            QRegularExpression::CaseInsensitiveOption);

        text.remove(commentRe);
        text.remove(tokenRe);
        QList<QssRule> rules;
        auto matches = ruleRe.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            QStringList properties;
            auto propertyMatches = propertyRe.globalMatch(match.captured(2));
            while (propertyMatches.hasNext())
                properties.append(propertyMatches.next().captured(1));
            if (!properties.isEmpty())
                rules.append({match.captured(1).trimmed(), properties});
        }
        return rules;
    }

    QString targetClass(QString selector) {
        static const QRegularExpression combinatorRe(QStringLiteral(R"re([\s>+~])re"));
        static const QRegularExpression suffixRe(QStringLiteral(R"re([#:\[.])re"));

        const auto parts = selector.split(combinatorRe, Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return {};
        auto name = parts.last();
        const auto cut = name.indexOf(suffixRe);
        if (cut >= 0)
            name.truncate(cut);
        // Qt spells namespaced class names as Namespace--Class in QSS.
        name.replace(QStringLiteral("--"), QStringLiteral("::"));
        return name.split(QStringLiteral("::")).last();
    }

} // namespace

void GuiComponentTests::themeQssPropertiesResolveForAllTargets() {
    const QDir sourceRoot(QStringLiteral(TEST_SOURCE_DIR));
    const auto classes = collectClassDeclarations(sourceRoot.filePath(QStringLiteral("src")));
    QVERIFY2(classes.size() > 300, qPrintable(QStringLiteral("Only %1 classes parsed from %2")
                                                  .arg(classes.size())
                                                  .arg(sourceRoot.path())));

    QSet<QString> declaredAnywhere;
    for (auto it = classes.cbegin(); it != classes.cend(); ++it)
        declaredAnywhere.unite(it->properties);

    // Properties inherited from a Qt base class are not visible in our headers.
    // List any such name here when a theme deliberately targets one.
    const QSet<QString> qtBuiltinProperties;

    const QDir themeRoot(sourceRoot.filePath(QStringLiteral("src/app/Resources/theme")));
    QStringList issues;
    auto fileCount = 0;
    auto resolvedCount = 0;
    QDirIterator it(themeRoot.path(), {QStringLiteral("*.qss")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next();
        ++fileCount;
        const auto text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const auto relative = themeRoot.relativeFilePath(path);
        for (const auto &rule : collectQssRules(text)) {
            for (const auto &property : rule.properties) {
                for (const auto &simple :
                     rule.selector.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                    const auto target = targetClass(simple);
                    if (target.isEmpty() || !classes.contains(target))
                        continue;
                    QSet<QString> visited;
                    if (resolvedProperties(classes, target, visited).contains(property)) {
                        ++resolvedCount;
                    } else if (declaredAnywhere.contains(property)) {
                        issues.append(
                            QStringLiteral("%1: [%2] sets qproperty-%3 on %4, which does not "
                                           "declare it (Qt warns on every polish)")
                                .arg(relative, rule.selector, property, target));
                    } else if (qtBuiltinProperties.contains(property)) {
                        ++resolvedCount;
                    } else {
                        issues.append(QStringLiteral("%1: [%2] sets undeclared qproperty-%3 on %4")
                                          .arg(relative, rule.selector, property, target));
                    }
                }
            }
        }
    }

    QVERIFY2(fileCount >= 10, qPrintable(QStringLiteral("Only %1 theme QSS files found under %2")
                                             .arg(fileCount)
                                             .arg(themeRoot.path())));
    QVERIFY2(resolvedCount > 100,
             qPrintable(QStringLiteral("Only %1 declarations could be verified; the QSS or header "
                                       "parser is probably broken")
                            .arg(resolvedCount)));
    QVERIFY2(issues.isEmpty(), qPrintable(issues.join(QLatin1Char('\n'))));
}
