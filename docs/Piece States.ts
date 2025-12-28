import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogCsAFmbYAzNIBMADmYrpANm3SAnKoA0IAJ6IAtAHZZ2NdLX7FARjUbmL-SoC+3k2iw8IjJKQGf0wAJfQFgVQAfPQBezQE5TFnYkEB5+IVFxKQQLaUV9bCtmKw1nZl0NK2kTcxzFNWw87WZZexVtdv0rHz8QAJwCEnJsQAB9QGnNQeIKQG+5QEKlQHoVQAqlJPE0wWExFOyLRW1FbBUtNStFZn19N201GsQvRulpDz19PUOXX38MAeDhwGNrQAp1AgzWaABCNAPlKKxSawym1A23a+1kihKNlkzgMaJuCBc2EM6PyzSsXW0VQ+fS+QSGqGwAKBc3BjGcyS4vHWmS2li02kKLm0aLU8j5sh0WJ2DSaLTaHW0XWcZP6lJCNMB+GBgEZNQCksZCWekNllObYhS0HrJ5MV9Josc50biHswDOdZCpnF41PKKZNhoABi0Ap+aAcNNAMYWgEIrQDR6hRADHaIO1qVZMP1CGtCj5TjO2mcRNUaax6m5wpNTkMzqsVndgU91N9gdDEajTNWsb1HITh1xKiRDxOBnTZyximR2GcKguROaal2+lL3yp2EAkt6AUP1AFOJgD5TCiAA2VABSugGPIphseu69lwmQqOwqPvSbrMF3OWQk0X1Rq7e2aNwldSySeK4axQANpiQa9HoUbQ8EFUfZnnNVQzmka9e2cW10VkJxFEQlQrFcD9y2wH8-0jRlmRjfdYUkRBWgKBx8lOdEPFaG8sXObA+VfNsnRaZxFB6T4yx+alsKmXCVHwwCD2IkDikaAV7ReM4VDOa4zEQZxdAOa88ntNxymYcoMPwUJAA2s8gqFQOgd0EhthOyOQFGUdRNB0Z5jHkhA01sC00Q6c99FkbpFG0vSDIiGIEgAsyiO2PICiKEpL3KK5im0LFhX2LpihsTy9hsdFfP0yhAAsIwAG50AbKVACHlQAHUyC3coRC+MxX2QcDD7E4NBlPQsSuY83EHdsbxOYssv88ISHK0zCOq8LCmKNRUMOZpDn0VrJrsNROuUbrkW0PrqXGSZgSWYKRqbHZoMaC4ulONsBWlLFJqsQoludDRmLRDblTpUEIQqnU2VCg04KWqDTxYi05scqoboca0kQtVxujdXoFQIPzqVpVV6QhOtKv24CLFPaQBxaElmEm8oLgc2o2sWwd0RkqpjhLOGPR07KXpR2ZNT2r7qrbBjTSecp0ycXRFCu1QDiON89gknz6a4xHsErYMw1wj6CI5ptByTRDlBffRBxfK1jWwNEiR1vYLilzjvll+Xq1w9HPrjNX2NbJ0TfkTX4scspPIOQlPM0doqnN8kZaZ+dlzXLcTL3VXgJFya+xvKnHCReb2rutjTUeGx1uly2mZ-PT-2VoTvpA08GIMCCZLxWQsVB8TrV16y+TlXOgllgvdP-O2VYd4DSPEt3TWdPk1A92oJOwcoHBkyzzzcZ7O--ATo77kSs-E-JEuRIHizolskVPU6PBTd824IQBGVUAVZt+sGxJi6qg6xsi67ps0IXPZkho0vRHXTh6s+FsgjXwMjQBg7M14WXkEoVQD07IGFJgpXYcEZQ8yLF7Qm2kQFhCiHEe+w0Y4iQsMlRol52JqXOO0Wunsbz7HPFUF4lCZJeSwTfXKhVSpDVXkBIhxxcbWiJPaRSqgiRWCtASRozwSREiNp5Vh-ldKcPwdw8ylhn4TTbCfTRKgrRyAaMUQmJJWjXhaPIzaEx8DXx2ssB+mMiHjinpKdiyEtDIWobURSl5GjXj2IY5Emkc5AMvmw5m6otS2MIfCG6N5waDj2M0RCbErQdAUNeKoo8rxoWkGY0JqMIE8PhKhAcxYprNEkqhdxClNZ2HOB0c4DgdatyCZYkJyNgQMh7iXTm15cT+0Dp5boBhx5VPLgKJ0RRKIlECcHb42DZyLhXBubc+TVEIEQvBYsyEdZjhksMhMTp9gyLRI8PYrh6g5OtorKMETIFIOPMmXYS0xwuCdJUhMxYbp6BMZs9SegLn+gVt3AhtyExOy8EiEkLwdCdWqJ7Mc3IDEWmUC8boTSZnAJCT+KxStgUFKPGBBpCS3DIW6LozQNSXjCkJlrJwOSsVXyBSo0uA9yKGHji6XYDxdHdCUE460zppJ0u-NiqMK8MaRJInyBiqFShOC0CIvZwiCheE8XwgUbYg7wxaV+YVDLcKKFxas4UDQOhWDagYY47FnDZlPIiQ4choJDjKGPbSgAHjUAGbaoCjLgJuXikC0DrI2Bii8XY2Y0THmRGUHeSJFLCjdZ6nBd8Vml2sBoW6wpTwuign2bMyJjyJ3Yhal0Qp439UCso8VILiFFFIWxWejgvB8mzKpBimhmhQqcEiUt7DiplQrfbP1Fg+EDn5kI14RI5K1GYqk40hYnlmths0j1CilHJtGsoA45pkJlBvJpa1jl7W40RTYSoY40VauXZQa+q7fWrNyBu6mKYdYlHTB-KdLaZppg0vUB4mqGaXtGBYj11i10HS2Y4xCbQ3y0QPR0Y8hZfEdCNhxdFBAANtLybelNhx7mbLNscQmjxJ2IFcUmNt15Opj3Pf+hNuS3p4SZd0hQ8hNECjcNXc8Ya0K8q0GbF4VxpkXtoxh1m4TDXYasuBR488KjAynXsWwVQETSLHI8P9XEAOXKLuJ+MaSBwoWUOiKZhjsxjmY5mTSaFNI627XLAFNtaw6bVl5Q2bZ5WB0HKI7MFp+EKuguOaC2Tz74AA2HRZkdQOx32PYRSjgxmxsMFxuCZ0LUEgE7Zn8wGcWMabKBCujgiVji8vu99pxW0IkOPIdihwMvfiy45nL-dHCbxTFRZCF1m0XEkZ5Z1jwLRaWCwBzL7rl5OdjmJVl29BHnGzAJn2TgJlpheChhU85AB7aiucYG3ItEPUA0F05FoLlGNYpG1Dwp5FC8B4M4MS6ZAPWyuPSN6xv2I8AOQwL6hz1GRdmTQBR5VlABmertbcHsUHyr2rhlbB3DoEccHzXlhy-fPAcB4ZQOXOpWxSMHEQk1YeqlFdNfXlBtR1tma0+x5XWbkPUZ5H4ce4KhwOu9JDHhsXYjrNnNgrTnBuloRJsoXj2A-BtwAjDqACu-CguPiBM97oOwnJQM2eGzW+xALzcywMHJQv7QWgFi8l09vtO3tjNSnlcf+Zr+YvF7ITfNIiCuoWsyLpcEvDLGWNzIG8dgHCaE0h4IoMHajq8NprocV2hy6-RfrqXjP+1y5ZzWtn5CGmNreexZr0E0rClNPIO7UeXeS4h89xrvCqgjsEdBcdFxexvFummPQY9G0nGd6769Rv8dPwfVutE+M9026KW0M1bYOgcrPr0EQ3AIBwHEP0Ev2x0fijIRzqi6Y3nYxOAOFxzodBTRp9pKkc+1H0WaloGS6Zmi6FFK4XGRXDgtCqycCcwXpxbWCIfnIaOGLLcgufmKySCiWaN56B9hNSR5arTjIzv7YzqDjS6DpjHDqwhr7wDh2jCj8ouKaD75KiXLv75CuYJynhU5XCeSwT3AIQ3h7DIiTTUZcTTgkCACYqYAPfRUBWg3Mue5CZmjwbYNqEaoMyCLw6Ixwi6qG3E8yy47+R08cha5wS0XQzoooXWeQTorgs0W+WBOqJA7+TqFcH2Eyikpwqu2IcEaOcgfIewTqYBFIgA0F6ADxeoAED6gAHHrv5BoHCGA6DHDKGvJX4izyDXj1BnDSRXAfiAAAqYAIPW7+PmPI3QQMZs9QYijkFgVMhsl4SIGgQM7QwhWqiMUBDouIVu9qFwTosKtQIim6-IRwSIhGz0r+mh0Od6n+z6TCFqRR+8N0xQR8+Q54Lopoz0kBdREmbRI4jgnilWOijkk0DQRRZqDwqExsWOIc3o9mIYuBBQByhmlQqKp2IMKOrgFOLoRQp4fywWssYWEhv0tqdkhBnUeyzkpC-IiOShmBxxTMekTBLBuMRoQ+0i8qiCCAJCuwsm5QL4cazxOqekWhFouIfsQaxonixRiA9cuxZQ0ocqDwZiOR9EjRXgvG6q8JzYG+joxiMoOGT+zScyr+18ORR0WJBRuJySewBwwxBxbEMh8xsyrSKoLBYklEqRbg3UqRuiLwSgXkeQnQF4k0OS18bx-RnMN+DeZ0p4xQ5+VoEy3iZwNgjuVQaJwWcypxMpTYrQMC8BIxcgKcnsyCrhw8qRMoDggmDMcyOB+pwEG6eIxYaIZQRQI+Vo1SiEIaVw149qdpXEcy9KEJCgjoWuASCW3pTghstSjyAZXI3aGJuM-IZwxaMoAiuargBwaEiUMmlhGmtGr+HqORLmaZOs-GjS8RU6aE3IjuZsQohw14tmfRzOEm7Ul44U4KAChh7QZQtoRwBguyC8g2tGjp7Z8Y0CaBug3UX68gcmJGLaMkRwAyRW9otmepk5uW7UlecGtkFOYxU68guMXR6IyJI+hZ3wAGHq0p25WMQ4vSyEBIQ4iSVw2Yj+DEVmskv6ewV5QQQ2dW7qEJqZBYXyXknQeJpGm6q5Xk65-5D2LBzWB2fJN4Dae8B6uxrapEakCINgLe4uORzom6RI14pojgSpNuC09oqEqCu6MUvgvgQAA */
    id: "片段状态",
    states: {
        "推理时长阶段": {
            states: {
                "开始推理时长": {
                    entry: ["创建推理任务", "销毁已有任务实例"],

                    on: {
                        "任务启动": "正在推理"
                    }
                },

                "正在推理": {
                    on: {
                        "任务成功": [{
                            target: "检查文档模型",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "时长错误"
                    },

                    exit: "销毁任务实例"
                },

                "检查文档模型": {
                    on: {
                        "完成": [{
                            target: "开始推理时长",
                            cond: "音素名称已更改"
                        }, "更新时长"]
                    }
                },

                "时长错误": {},

                "等待释放": {
                    on: {
                        "占用解除": "检查文档模型"
                    }
                },

                "更新时长": {
                    on: {
                        "完成": [{
                            target: "#片段状态.程序异常",
                            cond: "失败"
                        }, {
                            target: "#片段状态.销毁",
                            cond: "未找到片段"
                        }, {
                            target: "#片段状态.推理音高阶段",
                            cond: "成功"
                        }]
                    }
                }
            },

            initial: "开始推理时长",

            on: {
                "片段被移除": "销毁",

                "音素名称更改": ".开始推理时长"
            }
        },

        "程序异常": {
            type: "final"
        },

        "销毁": {
            type: "final"
        },

        "推理音高阶段": {
            states: {
                "开始推理时长": {
                    entry: ["创建推理任务", "销毁已有任务实例"],

                    on: {
                        "任务启动": "正在推理"
                    }
                },

                "正在推理": {
                    on: {
                        "任务成功": [{
                            target: "检查文档模型",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "音高错误"
                    },

                    exit: "销毁任务实例"
                },

                "检查文档模型": {
                    on: {
                        "完成": [{
                            target: "开始推理时长",
                            cond: "音素时长已更改"
                        }, "更新音高"]
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "检查文档模型"
                    }
                },

                "音高错误": {},

                "更新音高": {
                    on: {
                        "完成": [{
                            target: "#片段状态.程序异常",
                            cond: "失败"
                        }, {
                            target: "#片段状态.销毁",
                            cond: "未找到片段"
                        }, {
                            target: "#片段状态.推理唱法阶段",
                            cond: "成功"
                        }]
                    }
                }
            },

            initial: "开始推理时长",

            on: {
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",

                "表现力参数更改": ".开始推理时长",

                "音素时长更改": ".开始推理时长"
            }
        },

        "推理唱法阶段": {
            states: {
                "开始推理唱法": {
                    entry: ["创建推理任务", "销毁已有任务实例"],

                    on: {
                        "任务启动": "正在推理"
                    }
                },

                "正在推理": {
                    exit: "销毁任务实例",

                    on: {
                        "任务失败": "唱法错误",
                        "任务成功": [{
                            target: "检查文档模型",
                            cond: "文档模型未占用"
                        }, "等待释放"]
                    }
                },

                "唱法错误": {},

                "等待释放": {
                    on: {
                        "占用解除": "检查文档模型"
                    }
                },

                "检查文档模型": {
                    on: {
                        "完成": [{
                            target: "开始推理唱法",
                            cond: "音高已更改"
                        }, "更新唱法"]
                    }
                },

                "更新唱法": {
                    on: {
                        "完成": [{
                            target: "#片段状态.程序异常",
                            cond: "失败"
                        }, {
                            target: "#片段状态.销毁",
                            cond: "未找到片段"
                        }, {
                            target: "#片段状态.等待回放",
                            cond: "成功 & 延迟推理声学"
                        }, {
                            target: "#片段状态.推理声学阶段",
                            cond: "成功 & 不延迟推理声学"
                        }]
                    }
                }
            },

            initial: "开始推理唱法",

            on: {
                "音素时长更改": "推理音高阶段",
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": "推理音高阶段",

                "音高参数更改": ".开始推理唱法"
            }
        },

        "推理声学阶段": {
            states: {
                "开始推理声学": {
                    entry: ["创建推理任务", "销毁已有任务实例"],

                    on: {
                        "任务启动": "正在推理"
                    }
                },

                "正在推理": {
                    exit: "销毁任务实例",

                    on: {
                        "任务成功": [{
                            target: "检查文档模型",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "声学错误"
                    }
                },

                "检查文档模型": {
                    on: {
                        "完成": [{
                            target: "开始推理声学",
                            cond: "唱法已更改"
                        }, "更新声学"]
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "检查文档模型"
                    }
                },

                "声学错误": {},

                "更新声学": {
                    on: {
                        "完成": [{
                            target: "#片段状态.程序异常",
                            cond: "失败"
                        }, {
                            target: "#片段状态.销毁",
                            cond: "未找到片段"
                        }, {
                            target: "#片段状态.回放就绪",
                            cond: "成功"
                        }]
                    }
                }
            },

            initial: "开始推理声学",

            on: {
                "片段被移除": "销毁",
                "音素时长更改": "推理音高阶段",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": "推理音高阶段",
                "音高参数更改": "推理唱法阶段",

                "唱法参数更改": ".开始推理声学"
            }
        },

        "等待回放": {
            on: {
                "开始回放": "推理声学阶段",
                "音高参数更改": "推理唱法阶段",
                "表现力参数更改": "推理音高阶段",
                "音素时长更改": "推理音高阶段",
                "音素名称更改": "推理时长阶段"
            }
        },

        "回放就绪": {
            on: {
                "音素时长更改": "推理音高阶段",
                "音高参数更改": "推理唱法阶段",
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": "推理音高阶段",
                "唱法参数更改": "推理声学阶段"
            }
        }
    },

    initial: "推理时长阶段"
});