#!/usr/bin/env python3
"""
同步 Fluent UI System Icons 到项目 Resources/svg/icons/ 目录。

用法:
    python tools/sync_fluent_icons.py [--source-dir <fluent-icons-path>]

如果不指定 --source-dir，默认从 ../fluent-icons-tmp/package/icons/ 查找。
"""

import os
import shutil
import sys
import re
import argparse

# ============================================================
# 1. 需要替换的已有通用图标（用 Fluent 覆盖）
# ============================================================
REPLACE_ICONS = [
    # Playback / Transport
    "play_16_filled.svg",
    "pause_16_filled.svg",
    "stop_16_filled.svg",
    "next_16_filled.svg",
    "previous_16_filled.svg",
    "arrow_repeat_all_16_filled.svg",

    # Edit
    "arrow_undo_16_filled.svg",
    "arrow_redo_16_filled.svg",
    "cut_20_filled.svg",
    "copy_16_filled.svg",
    "clipboard_paste_16_filled.svg",
    "dismiss_16_filled.svg",
    "checkmark_16_filled.svg",

    # File
    "save_16_filled.svg",
    "save_edit_20_filled.svg",
    "document_add_16_filled.svg",
    "folder_open_16_filled.svg",
    "home_16_filled.svg",

    # UI / Navigation
    "search_16_filled.svg",
    "settings_16_filled.svg",
    "info_16_filled.svg",
    "chevron_down_16_filled.svg",
    "chevron_left_16_filled.svg",
    "chevron_right_16_filled.svg",
    "navigation_16_filled.svg",
    "more_horizontal_24_regular.svg",
    "globe_16_regular.svg",
    "color_16_filled.svg",
    "puzzle_piece_16_filled.svg",
    "window_apps_16_filled.svg",

    # Layout / Panels
    "layer_20_filled.svg",
    "panel_bottom_20_filled.svg",
    "panel_separate_window_20_filled.svg",
]

# ============================================================
# 2. 新增的 Fluent 图标（菜单、工具栏等）
# ============================================================
NEW_ICONS = [
    # File menu 新增
    "add_16_filled.svg",
    "delete_16_filled.svg",
    "rename_16_filled.svg",
    "arrow_import_16_filled.svg",
    "arrow_export_16_filled.svg",
    "folder_16_filled.svg",
    "clock_16_filled.svg",
    "document_16_filled.svg",
    "select_all_on_16_filled.svg",

    # 编辑
    "tag_16_filled.svg",
    "link_16_filled.svg",
    "question_circle_16_filled.svg",
    "add_circle_16_filled.svg",

    # 录音
    "record_16_filled.svg",
    "timer_16_filled.svg",
    "mic_16_filled.svg",
    "mic_off_16_filled.svg",
    "headphones_16_filled.svg",
    "arrow_repeat_all_off_16_filled.svg",

    # 工具 (24px)
    "zoom_in_24_filled.svg",
    "zoom_out_24_filled.svg",
    "full_screen_maximize_24_filled.svg",
    "full_screen_minimize_24_filled.svg",
    "timer_24_filled.svg",
    "mic_24_filled.svg",
    "mic_off_24_filled.svg",
    "headphones_24_filled.svg",
    "music_note_2_16_filled.svg",
    "music_note_2_24_filled.svg",
    "draw_shape_24_filled.svg",

    # 分割/合并 (箭头分叉和合并)
    "arrow_split_24_filled.svg",
    "arrow_split_16_filled.svg",
    "arrow_join_20_filled.svg",

    # 面板
    "panel_left_16_filled.svg",
    "panel_left_contract_16_filled.svg",
    "panel_left_expand_16_filled.svg",
    "panel_right_16_filled.svg",

    # 录音
    "record_24_filled.svg",
]

# ============================================================
# 3. 需要删除的冗余文件（_white 和 _alt 变体，IconUtils 可以着色）
# ============================================================
DELETE_REDUNDANT = [
    # _white 变体 — IconUtils 可运行时着色，不需要预存白色版
    "add_16_filled_white.svg",
    "checkmark_16_white.svg",
    "info_16_filled_white.svg",
    "settings_16_filled_white.svg",
    "chevron_down_16_filled_white.svg",
    "chevron_left_16_filled_white.svg",
    "chevron_right_16_filled_white.svg",
    "midi_clip_16_filled_white.svg",
    "audio_clip_16_filled_white.svg",

    # 工具图标的白/alt变体 — 只保留主版本
    "cursor_24_filled_white.svg",
    "edit_24_filled_white.svg",
    "eraser_24_filled_white.svg",
    "beam_24_filled_white.svg",
    "pitch_anchor_24_filled_white.svg",
    "pitch_edit_24_filled_white.svg",
    "pitch_erase_24_filled_white.svg",
    "pitch_freeze_24_filled_white.svg",
    "pitch_freeze_24_filled_alt.svg",
    "pitch_freeze_24_filled_white_alt.svg",
    "phoneme_view_24_filled_white.svg",
    "param_view_24_filled_white.svg",
    "param_view_24_filled_alt.svg",
    "param_view_24_filled_white_alt.svg",
    "panel_maximize_24_filled_white.svg",
    "panel_hide_24_filled_white.svg",
    "snow_flake_24_filled_white.svg",
    "restore_16_filled_white_alt.svg",
]

# ============================================================
# 执行
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="同步 Fluent UI 图标到项目")
    parser.add_argument("--source-dir", default=None,
                        help="Fluent 图标目录，默认自动查找")
    args = parser.parse_args()

    # 脚本所在目录 (tools/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # 项目根目录
    project_root = os.path.dirname(script_dir)
    # 目标目录
    icons_dir = os.path.join(project_root, "src", "app", "Resources", "svg", "icons")
    icons_custom_dir = os.path.join(project_root, "src", "app", "Resources", "svg", "icons-custom")

    # 源目录
    if args.source_dir:
        source_dir = args.source_dir
    else:
        source_dir = os.path.join(project_root, "..", "fluent-icons-tmp", "package", "icons")

    if not os.path.isdir(source_dir):
        print(f"✗ Fluent 源目录不存在: {source_dir}")
        print("  请指定 --source-dir 或先下载 Fluent 图标包")
        sys.exit(1)

    if not os.path.isdir(icons_dir):
        print(f"✗ 目标目录不存在: {icons_dir}")
        sys.exit(1)

    print(f"Fluent 源: {source_dir}")
    print(f"目标目录: {icons_dir}")
    print()

    # ---- 第一步：替换已有通用图标 ----
    copied = 0
    missing = []
    for name in REPLACE_ICONS:
        src = os.path.join(source_dir, name)
        dst = os.path.join(icons_dir, name)
        if os.path.isfile(src):
            shutil.copy2(src, dst)
            copied += 1
            print(f"  ✓ REPLACE {name}")
        else:
            missing.append(name)
            print(f"  ✗ MISSING  {name} (fluent 中没有)")

    # ---- 第二步：复制新增图标 ----
    for name in NEW_ICONS:
        src = os.path.join(source_dir, name)
        dst = os.path.join(icons_dir, name)
        if os.path.isfile(src):
            shutil.copy2(src, dst)
            copied += 1
            print(f"  ✓ ADD     {name}")
        else:
            missing.append(name)
            print(f"  ✗ MISSING  {name} (fluent 中没有)")

    # ---- 第三步：删除冗余文件 ----
    deleted = 0
    for name in DELETE_REDUNDANT:
        fpath = os.path.join(icons_dir, name)
        if os.path.isfile(fpath):
            os.remove(fpath)
            deleted += 1
            print(f"  ✓ DELETE  {name}")
        else:
            print(f"  - SKIP    {name} (不存在)")

    # ---- 第四步：检查 icons-custom/ 中是否也有冗余 ----
    for name in DELETE_REDUNDANT:
        fpath = os.path.join(icons_custom_dir, name)
        if os.path.isfile(fpath):
            os.remove(fpath)
            deleted += 1
            print(f"  ✓ DELETE  icons-custom/{name}")

    print()
    print(f"=== 完成 ===")
    print(f"  复制/替换: {copied} 个图标")
    print(f"  删除冗余:  {deleted} 个文件")
    if missing:
        print(f"  缺失(Fluent无): {len(missing)} 个")
        for m in missing:
            print(f"    - {m}")
    print()


if __name__ == "__main__":
    main()