#ifndef GHOSTNOTESOURCE_H
#define GHOSTNOTESOURCE_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>

class SingingClip;

// 其他轨道上的一个参考音符（ghost note）。时间用绝对 tick 表达，
// 由各渲染后端自行减去当前 clip 的 start() 换算到场景坐标。
struct GhostNote {
    int globalStart = 0;
    int length = 0;
    int keyIndex = 60;
    int colorIndex = 0; // 源轨道 colorIndex
};

// 两套渲染后端共用的 ghost note 视觉参数与配色
namespace GhostNoteStyle {
    inline constexpr double heightRatio = 0.2; // 占琴键行高的比例
    inline constexpr double minHeight = 2.0;   // 逻辑像素下限
    inline constexpr double opacity = 0.45;    // 相对 noteBackground 的 alpha 系数

    QColor fillColor(int colorIndex);
}

// 收集「除宿主轨道外所有 singing clip 的音符」，并在模型或外观选项变化时刷新。
// Legacy（GhostNoteOverlay）与 RHI（PianoRollRhiWidget）各持一份实例。
class GhostNoteSource final : public QObject {
    Q_OBJECT

public:
    explicit GhostNoteSource(QObject *parent = nullptr);

    // 当前钢琴卷帘正在编辑的 clip，nullptr 表示无 clip（清空）
    void setHostClip(SingingClip *clip);
    // 选项已开启且存在宿主 clip
    [[nodiscard]] bool enabled() const;
    // 按 globalStart 升序
    [[nodiscard]] const QList<GhostNote> &notes() const;
    // 列表中最长音符的长度，供可见区间二分定位起点使用
    [[nodiscard]] int maxLength() const;

signals:
    void changed();

private:
    void scheduleRebuild();
    void rebuild();
    void rebuildConnections();
    void clearConnections();

    QPointer<SingingClip> m_hostClip;
    QList<GhostNote> m_notes;
    int m_maxLength = 0;
    bool m_enabled = false;
    bool m_rebuildScheduled = false;
    bool m_connectionsBuilt = false;
};

#endif // GHOSTNOTESOURCE_H
