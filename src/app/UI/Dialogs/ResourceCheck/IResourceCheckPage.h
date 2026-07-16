#ifndef IRESOURCECHECKPAGE_H
#define IRESOURCECHECKPAGE_H

#include <QWidget>

// Page interface of the resource check dialog. One page per resource type (audio files, singer packages, etc.),
// assembled on demand by ResourceCheckDialog; implement this interface to add new resource types
class IResourceCheckPage : public QWidget {
    Q_OBJECT

public:
    explicit IResourceCheckPage(QWidget *parent = nullptr) : QWidget(parent) {
    }

    [[nodiscard]] virtual QString title() const = 0;
    [[nodiscard]] virtual bool hasPendingIssues() const = 0;
};

#endif // IRESOURCECHECKPAGE_H
