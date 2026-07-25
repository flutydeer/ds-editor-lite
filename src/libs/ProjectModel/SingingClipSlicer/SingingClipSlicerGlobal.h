//
// Created by FlutyDeer on 2025/11/25.
//

#ifndef DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H
#define DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H

namespace SingingClipSlicerGlobal {
    // TODO: 更改为实体类
    // 头部可用区域最大值(ms)
    constexpr double headerAvailableLengthMax = 1000;

    // 头部填充SP长度基础量(ms)
    constexpr double padBaseLength = 100;

    // 头部填充SP长度增量基数(ms)
    constexpr double padUnitAdditionalLength = 100;
}

#endif //DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H