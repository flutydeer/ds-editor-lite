#ifndef SEGMENTEDTABBAR_H
#define SEGMENTEDTABBAR_H

#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QPushButton;

// Fixed-width bar of mutually exclusive checkable push buttons that replaces
// QTabBar in TabPanelTitleBar. The Fixed size policy keeps the panel layout
// from squeezing the segment labels when the window narrows.
class SegmentedTabBar : public QWidget {
    Q_OBJECT

public:
    explicit SegmentedTabBar(QWidget *parent = nullptr);

    int addTab(const QString &text);
    [[nodiscard]] int count() const;
    [[nodiscard]] QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    [[nodiscard]] int currentIndex() const;
    void setCurrentIndex(int index);
    [[nodiscard]] QRect tabRect(int index) const;

signals:
    void currentChanged(int index);

private:
    void updateSegmentRoles();

    QHBoxLayout *m_layout;
    QButtonGroup *m_buttonGroup;
    QList<QPushButton *> m_buttons;
    int m_currentIndex = -1;
};

#endif // SEGMENTEDTABBAR_H
