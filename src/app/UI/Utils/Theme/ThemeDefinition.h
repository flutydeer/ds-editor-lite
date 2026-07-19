#ifndef THEMEDEFINITION_H
#define THEMEDEFINITION_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>

struct ThemeDefinition {
    QString folderName; // theme ID = folder name on disk
    QString name;       // display name from manifest
    QString author;     // author from manifest
    QString colorType;  // "light" | "dark" | "highContrast"

    QString styleSheet;                    // combined QSS from all styleSheets files
    QList<QColor> paletteColors;           // parsed app-color-palette.json
    QString lyricStyleSheet;               // content of lyricStyleSheet file
    QHash<QString, QColor> semanticColors; // resolved semantic tokens from colors.json
};

#endif // THEMEDEFINITION_H
