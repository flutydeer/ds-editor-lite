#ifndef AUDIORESOURCEPAGE_H
#define AUDIORESOURCEPAGE_H

#include "IResourceCheckPage.h"

class QTreeWidget;
class QTreeWidgetItem;

// Audio page of the resource check dialog: lists missing/unconfirmed audio clips,
// supports per-item relinking (with cascading resolution of remaining missing items) and confirming name-matched items
class AudioResourcePage final : public IResourceCheckPage {
    Q_OBJECT

public:
    explicit AudioResourcePage(const QList<int> &missingClipIds,
                               const QList<int> &unconfirmedClipIds, QWidget *parent = nullptr);

    [[nodiscard]] QString title() const override;
    [[nodiscard]] bool hasPendingIssues() const override;

private:
    enum class RowStatus { Missing, Unconfirmed, Resolved };

    void addRow(int clipId, RowStatus status);
    void setRowStatus(QTreeWidgetItem *item, RowStatus status) const;
    QTreeWidgetItem *findRowByClipId(int clipId) const;
    void onRelocateClicked();
    void onConfirmClicked();
    void updateActionButtons() const;

    QTreeWidget *m_tree = nullptr;
    class Button *m_btnRelocate = nullptr;
    class Button *m_btnConfirm = nullptr;
};

#endif // AUDIORESOURCEPAGE_H
