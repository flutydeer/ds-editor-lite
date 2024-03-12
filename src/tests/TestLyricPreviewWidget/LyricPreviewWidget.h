#ifndef LYRICPREVIEWWIDGET_H
#define LYRICPREVIEWWIDGET_H

#include <QGraphicsView>

class QLineEdit;
class QComboBox;

class CellGraphicsItem;
class LyricPreviewWidgetPrivate;
class LyricPreviewWidgetCellPrivate;
class LyricPreviewWidget;

class LyricPreviewWidgetCell : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(LyricPreviewWidgetCell)
public:
    enum Role {
        Lyric,
        Pronunciation,
    };

    enum State {
        Normal = 0,
        Hovered = 1,
        Selected = 2,
    };

    explicit LyricPreviewWidgetCell(LyricPreviewWidget *parent = nullptr);
    LyricPreviewWidgetCell(const QString &lyric, const QString &pronunciation, const QStringList &pronunciationCandidates, LyricPreviewWidget *parent = nullptr);
    ~LyricPreviewWidgetCell() override;

    void setLyric(const QString &lyric);
    QString lyric() const;

    void setPronunciation(const QString &pronunciation);
    QString pronunciation() const;

    void setPronunciationCandidates(const QStringList &pronunciation);
    QStringList pronunciationCandidates() const;

    void setBackground(State state, const QBrush &brush);
    QBrush background(State state) const;

    void setBorder(State state, const QPen &border);
    QPen border(State state) const;

    void setForeground(State state, Role role, const QPen &pen);
    QPen foreground(State state, Role role);

    void setSelected(bool selected);
    bool isSelected() const;

    void setActivated(bool activated);
    bool isActivated() const;

    void setToolTip(const QString &toolTip);
    QString toolTip() const;

protected:
    LyricPreviewWidgetCell(LyricPreviewWidget *parent, LyricPreviewWidgetCellPrivate &d);

private:
    friend class CellGraphicsItem;
    friend class LyricPreviewWidget;
    friend class LyricPreviewWidgetPrivate;
    QScopedPointer<LyricPreviewWidgetCellPrivate> d_ptr;
};

class LyricPreviewWidget : public QGraphicsView {
    Q_OBJECT
    Q_DECLARE_PRIVATE(LyricPreviewWidget)
public:
    explicit LyricPreviewWidget(QWidget *parent = nullptr);
    ~LyricPreviewWidget() override;

    void insertCell(int row, int column, LyricPreviewWidgetCell *cell);
    void insertRow(int row, const QList<LyricPreviewWidgetCell *>& cells = {});
    void removeCell(LyricPreviewWidgetCell *cell);
    void removeRow(int row);

    int rowCount() const;
    int columnCount(int row) const;


    LyricPreviewWidgetCell *cell(int row, int column) const;
    LyricPreviewWidgetCell *cellAt(const QPoint &point) const;
    inline LyricPreviewWidgetCell *cellAt(int x, int y) const {
        return cellAt({x, y});
    }
    QList<LyricPreviewWidgetCell *> cells() const;

    LyricPreviewWidgetCell *currentCell() const;
    void setCurrentCell(LyricPreviewWidgetCell *cell);

    QList<LyricPreviewWidgetCell *> selectedCells() const;

    void setSplitter(const QPen &splitter);
    QPen splitter() const;

    void setVerticalMargin(qreal margin);
    qreal verticalMargin() const;

    void setHorizontalMargin(qreal margin);
    qreal horizontalMargin() const;

    void setCellPadding(qreal padding);
    qreal cellPadding() const;

signals:
    void currentCellChanged(LyricPreviewWidgetCell *current, LyricPreviewWidgetCell *previous);
    void cellChanged(LyricPreviewWidgetCell *cell);
    void cellActivated(LyricPreviewWidgetCell *cell, LyricPreviewWidgetCell::Role role);
    void cellSelectionChanged();

protected:
    LyricPreviewWidget(QWidget *parent, LyricPreviewWidgetPrivate &d);
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class CellGraphicsItem;
    QScopedPointer<LyricPreviewWidgetPrivate> d_ptr;
};

#endif // LYRICPREVIEWWIDGET_H
