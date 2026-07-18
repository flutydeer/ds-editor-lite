# 主题 qproperty 属性清单

本清单罗列 `src/app/Resources/theme/lite-dark/*.qss` 中通过 `qproperty-*` 写入 C++ Q_PROPERTY 的全部属性，作为**新主题的必填清单**。

## 为什么必须填全

`qproperty-*` 是**一次性赋值**：从主题 A 切换到主题 B 时，B 漏写的属性会**残留 A 的值**，不会自动回到 C++ 默认值。C++ 中的默认值仅在"QSS 完全没有该规则"时作为首次加载的回退，不能作为主题切换的回退。因此：

- 新主题必须覆盖下表中的**每一项**；
- 主题切换测试应使用"深 → 浅 → 深"往返验证，而不是分别启动各主题；
- 将来做多主题时建议加自动校验脚本，对比各主题声明的 qproperty 集合是否一致；
- 新增 qproperty 时须同步更新本清单。

值列为 lite-dark 当前值，供新主题参考。标注（非颜色）的属性为尺寸/开关类，通常各主题取相同值，但同样必须显式写出。

## controls.qss

### SVS--SeekBar（SVS::SeekBar）
| 属性 | lite-dark 值 |
|---|---|
| trackInactiveColor | #60FFFFFF |
| trackActiveColor | #9BBAFF |
| thumbFillColor | #9BBAFF |
| thumbBorderColor | #454545 |

### TimeGraphicsView
| 属性 | lite-dark 值 |
|---|---|
| barLineColor | #08090A |
| beatLineColor | #16191C |
| commonLineColor | #1C2024 |
| playPosIndicatorColor | #C8C8C8 |
| lastPlayPosIndicatorColor | #A0A0A0 |
| scrollBarHandleColor | #FFFFFF |
| rubberBandBorderColor | rgba(155, 186, 255, 200) |
| rubberBandFillColor | rgba(155, 186, 255, 64) |

### DividerLine
| 属性 | lite-dark 值 |
|---|---|
| lineMargin（非颜色） | 12px |
| lineColor | #1D1F26 |

### OptionsCardItem>DividerLine
| 属性 | lite-dark 值 |
|---|---|
| lineMargin（非颜色） | 0px |

### SeekBar（全局命名空间）
| 属性 | lite-dark 值 |
|---|---|
| trackInactiveColor | rgba(0, 0, 0, 32) |
| trackActiveColor | #709CFF |
| thumbBorderColor | #FFFFFF |

### SwitchButton
| 属性 | lite-dark 值 |
|---|---|
| trackOffColor | rgba(255, 255, 255, 16) |
| trackOnColor | #9BBAFF |
| thumbOffColor | #FFFFFF |
| thumbOnColor | #000000 |

### ProgressIndicator
| 属性 | lite-dark 值 |
|---|---|
| inactiveColor | rgba(255, 255, 255, 72) |
| normalTotalColor | #9BBAFF |
| normalSecondaryColor | #9FBDFF |
| normalCurrentTaskColor | #71DAFF |
| warningTotalColor | #FFCD9B |
| warningSecondaryColor | #FFCC99 |
| warningCurrentTaskColor | #FFDA71 |
| errorTotalColor | #FF9B9D |
| errorSecondaryColor | #FFABAD |
| errorCurrentTaskColor | #FFABDD |

### OverlayScrollBar
| 属性 | lite-dark 值 |
|---|---|
| handleColor | #FFFFFF |

### SplitterOverlayGrip
| 属性 | lite-dark 值 |
|---|---|
| highlightColor | rgba(255, 255, 255, 80) |

### TrackColorSwatchWidget
| 属性 | lite-dark 值 |
|---|---|
| selectedBorderColor ※ | #FFFFFF |
| hoverBorderColor ※ | rgba(255, 255, 255, 128) |

### LevelMeter
| 属性 | lite-dark 值 |
|---|---|
| dimmedColor | rgb(41, 44, 54) |
| clippedColor | rgb(255, 155, 157) |
| safeColor | rgb(155, 224, 255) |
| warnColor | rgb(155, 255, 174) |
| criticalColor | rgb(255, 224, 155) |
| currentValueColor | rgb(211, 214, 224) |
| peakHoldColor | rgb(211, 214, 224) |
| valueBackColor | rgba(33, 36, 43, 192) |

### TimelineView
| 属性 | lite-dark 值 |
|---|---|
| playheadColor | rgb(200, 200, 200) |
| barScaleColor | rgb(200, 200, 200) |
| barTickColor | rgb(92, 96, 100) |
| beatScaleColor | rgb(160, 160, 160) |
| beatTickColor | rgb(72, 75, 78) |
| subdivisionFromColor | rgb(76, 79, 83) |
| subdivisionToColor | rgb(57, 59, 61) |
| loopMarkerColor | #9BBAFF |
| loopMarkerDisabledColor | rgb(57, 59, 61) |
| piecePendingColor | rgb(100, 100, 100) |
| pieceRunningColor | rgb(255, 204, 153) |
| pieceSuccessColor | rgb(155, 255, 162) |
| pieceFailedColor | rgb(255, 155, 157) |

### TabPanelTitleBar
| 属性 | lite-dark 值 |
|---|---|
| iconColor | rgb(240, 240, 240) |
| iconDisabledColor | rgba(240, 240, 240, 102) |
| iconOnColor | #9BBAFF |
| iconOnDisabledColor | rgba(155, 186, 255, 102) |

## popups.qss

### ToastWidget
| 属性 | lite-dark 值 |
|---|---|
| shadowColor | rgba(0, 0, 0, 32) |

### ToolTip
| 属性 | lite-dark 值 |
|---|---|
| shadowColor | rgba(0, 0, 0, 32) |

## title-bar.qss

### TempoPopupWidget TapTempoButton#btnTapTempo
| 属性 | lite-dark 值 |
|---|---|
| progressColor | rgba(155, 186, 255, 80) |

### MainMenuView
| 属性 | lite-dark 值 |
|---|---|
| iconColor | #E0E0E0 |
| iconDisabledColor | #909090 |

### FilePopupWidget
| 属性 | lite-dark 值 |
|---|---|
| iconColor | #C8C9CC |

### PlaybackView
| 属性 | lite-dark 值 |
|---|---|
| playAccentColor | #9BBAFF |
| pauseAccentColor | #FFCD9B |

## track-editor.qss

### TrackControlView>LevelMeter
| 属性 | lite-dark 值 |
|---|---|
| padding（非颜色） | 0 |
| spacing（非颜色） | 1 |
| clipIndicatorLength（非颜色） | 6 |
| showValueWhenHover（非颜色） | false |
| dimmedColor | transparent |
| clippedColor | rgb(255, 155, 157) |
| safeColor | rgb(155, 224, 255) |
| warnColor | rgb(155, 255, 174) |
| criticalColor | rgb(255, 224, 155) |
| currentValueColor | rgb(211, 214, 224) |

### TracksGraphicsView
| 属性 | lite-dark 值 |
|---|---|
| selectedTrackColor | #31353F |
| clipSelectedBorderColor ※ | #FFFFFF |

## clip-editor.qss

### PianoKeyboardView
| 属性 | lite-dark 值 |
|---|---|
| whiteKeyColor | #DADBE0 |
| blackKeyColor | #3B3F47 |
| dividerColor | #AAACB5 |

### PianoRollGraphicsView
| 属性 | lite-dark 值 |
|---|---|
| noteFontPixelSize（非颜色） | 13 |
| whiteKeyColor | #292C36 |
| blackKeyColor | #23262E |
| octaveDividerColor | #1C2024 |
| noteSelectedBorderColor ※ | #FFFFFF |
| pronunciationTextColor | #C8C8C8 |
| anchorColor | #DCDCDC |
| anchorSelectedColor | #9BBAFF |
| anchorCurveColor | #DCDCDC |
| anchorPreviewColor | #9BBAFF |
| clipRangeOverlayColor | rgba(0, 0, 0, 64) |
| splitLineColor | #FF6464 |
| paramGraduateColor | #484B4E |
| paramOriginalCurveColor | rgba(255, 255, 255, 96) |
| paramEditedCurveColor | #FFFFFF |
| paramBackgroundLayerColor | #292C36 |

### ParamEditorGraphicsView
| 属性 | lite-dark 值 |
|---|---|
| paramGraduateColor | #484B4E |
| paramOriginalCurveColor | rgba(255, 255, 255, 96) |
| paramEditedCurveColor | #FFFFFF |
| paramBackgroundLayerColor | #292C36 |
| speakerMixTextColor | #DCDCDC |
| speakerMixKeyframeLineColor | #DCDCDC |
| speakerMixSelectedDotColor ※ | #FFFFFF |
| speakerMixSelectionBorderColor | rgba(155, 186, 255, 200) |
| speakerMixSelectionFillColor | rgba(155, 186, 255, 40) |

### PhonemeView
| 属性 | lite-dark 值 |
|---|---|
| hintTextColor | rgba(255, 255, 255, 80) |
| textColor | rgb(180, 180, 180) |
| positionLineColor | rgb(200, 200, 200) |
| noteBoundaryColor | rgb(49, 53, 63) |
| waveformColor | rgb(49, 53, 63) |

### ClipEditorToolBarView
| 属性 | lite-dark 值 |
|---|---|
| iconColor | rgb(240, 240, 240) |
| iconDisabledColor | rgba(240, 240, 240, 102) |
| iconOnColor | #9BBAFF |
| iconOnDisabledColor | rgba(155, 186, 255, 102) |

## mix-console.qss

### Fader
| 属性 | lite-dark 值 |
|---|---|
| trackInactiveColor | rgb(22, 22, 22) |
| thumbFillColor | rgb(211, 214, 224) |

### PanSlider
| 属性 | lite-dark 值 |
|---|---|
| centerGraduateColor | rgb(22, 22, 22) |

## dialogs.qss

### SpeakerMixBar
| 属性 | lite-dark 值 |
|---|---|
| trackColor | #2A2E38 |
| segmentTextColor | #111318 |
| dividerColor | #21242B |
| dividerDraggingColor | #FFFFFF |

## 过渡属性（※）

标 ※ 的属性画在 AppColorPalette 的 12 种业务色之上，需要与全部业务色保持对比度，单一 QSS 值在浅色主题下未必对所有色块可用。将来可能改为从 AppColorPalette 按色号推导（如 `palette.clipSelectionBorder(index)`）或按对比度计算，届时从本清单移除。对应代码处均有 `// NOTE: transitional` 注释。

## 不在本清单范围的颜色

- **AppColorPalette 业务色**：轨道色、音符色、说话人色等按色号取值的业务语义色，走 `AppColorPalette` + JSON，不经 QSS；
- **交互 alpha 档位**：悬停/按下/激活等状态透明度、动画插值端点保留在 C++，QSS 只提供基色；
- **FillLyric 模块**：已有独立的 QssParser + `lyricwrapview-dark.qss` 通道。
