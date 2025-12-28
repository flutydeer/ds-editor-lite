import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout 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-g5TGWgnimKqF-FQ+U61wWya2ss+aKAO07ZGi5btynh29heGpiaEDBOgheKcQolJ2CcChNtmlSaAvBllNxhQPGTJKp4HxmzR17HHeBVobxp3ZsurO9tUQ4hnKBcumJ+t4lGySYzEqvIDilHylDWud8v2nsAG96gB9+NyWohIgAuOQADogEAPRmuHi3wlWljTGcbkw2U2XW0ZrpVUS36njZR+bACMOoAK78z2OKIzYa9JQFVvHvSS7EH7eQKoHU4UdCHKQsfYx2zpS7u3WGMLyfqWhiho3DiYITvV-G+1dB0axbpPnSfXJuLjXZKLursC4EeC9KJCZHKMt6iFEkSZQsZz8-6zPRKKHrOJhtEkm1TFjV0yhow5j0DoLNUmqxsfnQUuTziFNlI9pU6pxIt1YxYrcn2EjDBHDczF9jRNF2JaiXkcuLhtp0KuQ8oLI8YwNEqLVdmuhdAoViIABtMSAUEADHaIIzNTnLsHeCDkQptGSUFxQ+JDwXEaXIXaqh2tdcmH1sVpXIxTmggcYU7M3jHRMamW4HENrwxeIxz5nXuurdUPJiGm2KqCnjG8DQ-YgGiKqQObQJhdAtq-Z1lcvX+tdru3aYbv0ihVKtHUcqBxbRyHh6Rwzf2OsA9W3A853aPtrRPuD72UPEAeL2JVOQuC9A5SW6j-rN31uPVBzjqCePsSYwzGcOHaNtAcSWz3VbwONvcmx7t8KhKsZAKqD2UBPLeo4y54TQHa3imY-52DoXlwRcNyePYJ4wapo5R4cozr3Oqe3cjGFNaVkg2WmcOiM+lxZbPF1xeOw53keG8YJoY3nZ9DQVFA0StN4re+rqOmGMU58gaBG3K53lJOtYp5x74WnVleziFEcLl8KlIc20Gdec+uOux-6+jwDmPE-0+T3Ks18FnkZ8xD6wwS38+MGpwru7JfBdl9T5sooymzxVJ9f1PIvgFQiG4BAOA4gvg09dvlSvsSDYJONhNuo2Ujhlthm8AKDxDl2upJPkuWvZZwxcJ7TazCl-wXLlofQNsdDInyGpakGKSC756VUg-3Ij8MLomfYe5klMkuspJirA-nNM-pKo0F3ltIcEzCYKSrcI2o0raHWi4IttviqD6L6KAfqsqsHG0AjOiHRJ4LWrIp9kfqcE8NzKgQ6pWFWJga-s9Kqqzl0DROKGcE0PoPBNUEVCgZ8oANBegA8XqABA+oABx6mBqI9g2Mykhw7B+gi+Ng6I-iQ4yYJwJwhgBYyigAAKmACD1pgSVOXFzNjCcK8g0PXGft2DoFVEeioa0namhJgYpgcNgnGDeC0NiL7HsPeKUNtnPFHirG5GMBMHYeOm-orAODlIHjcGUGwuUPkPkAONGDnp8rxA1AQHYZVlESYK6DJM4PoJFEgvoAOHumUMSMejYUlHwrQYoM9JxFjIhHZkNLeFOIKGxBPFUIDMuH6KkdgUFFfK9AQeEQgLrDtDXFmGjAKirI-IEacHiBpvsGcPkYzGULsKcHoAhMYOmEjoKiQv4avIEa-uzMUbMQev0eaLtETvsVUBzKcKUFAmqKkcUOkRjFkbrkAlNIUCOLaEKJ7Cap3GQhgc3itF0cfHgeeIQSwjRGtM6KKDoPoF6rwtQZga0KLF8VkYeEiGnIoNjGThRpImMWimEpMQUPsYkqqkcb4pXmUO4OVrcoAXiUMNseorsVNjMSSfMZ3uaPiicEUBzGXLSnauiiAf8VTGkcUBkezOks8ZsuJPaJ0JvjoKcFFirOiuUYKcLCNB6h0MgVUnEnUgmDBLaFUtXDRDSbMsKmGn8RjlEuCSaK8AhOLDaiYUmt2GaLoCVEmLbMom2qkSFo4PDBoDAdjF-psuiNBKmpOMYEiLyUZoVoEeCZVpjA0JaEUKStCmwrpqyiKBHLnk-iqcZAYXiG3JcWeDbPZncOiFCXpiNj4YEP9suJgXmcsQ8FAbNp4LIQgOVPBPqejAOCoFWTgAboTHWSVPmTfk2ZUJpurmLh4CYCsa8KOgqdWXnmooOfYA2cYfBGOafkMqku4VxAmLKHRIPt4EAA */
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
                            target: "#片段状态.更新时长",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "时长错误"
                    },

                    exit: "销毁任务实例"
                },

                "时长错误": {
                    on: {
                        "重试": "开始推理时长"
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "#片段状态.更新时长"
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
                            target: "#片段状态.更新音高",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "音高错误"
                    },

                    exit: "销毁任务实例"
                },

                "等待释放": {
                    on: {
                        "占用解除": "#片段状态.更新音高"
                    }
                },

                "音高错误": {
                    on: {
                        "重试": "开始推理时长"
                    }
                }
            },

            initial: "开始推理时长",

            on: {
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": ".开始推理时长",
                "音素时长更改": ".开始推理时长",
                "音高步数更改": ".开始推理时长"
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
                            target: "#片段状态.更新唱法",
                            cond: "文档模型未占用"
                        }, "等待释放"]
                    }
                },

                "唱法错误": {
                    on: {
                        "重试": "开始推理唱法"
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "#片段状态.更新唱法"
                    }
                }
            },

            initial: "开始推理唱法",

            on: {
                "音素时长更改": "推理音高阶段",
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": "推理音高阶段",
                "音高参数更改": ".开始推理唱法",
                "唱法步数更改": ".开始推理唱法"
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
                            target: "#片段状态.更新声学",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "声学错误"
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "#片段状态.更新声学"
                    }
                },

                "声学错误": {
                    on: {
                        "重试": "开始推理声学"
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
                "唱法参数更改": ".开始推理声学",
                "深度更改": ".开始推理声学",
                "声学步数更改": ".开始推理声学"
            }
        },

        "等待回放": {
            on: {
                "开始回放": "推理声学阶段",
                "音高参数更改": "推理唱法阶段",
                "表现力参数更改": "推理音高阶段",
                "音素时长更改": "推理音高阶段",
                "音素名称更改": "推理时长阶段",
                "延迟推理声学更改为\"是\"": "推理声学阶段"
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
        },

        "更新时长": {
            on: {
                "完成": [{
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }, {
                    target: "推理音高阶段",
                    cond: "成功"
                }]
            }
        },

        "更新音高": {
            on: {
                "完成": [{
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }, {
                    target: "推理唱法阶段",
                    cond: "成功"
                }]
            }
        },

        "更新唱法": {
            on: {
                "完成": [{
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }, {
                    target: "等待回放",
                    cond: "成功 & 延迟推理声学"
                }, {
                    target: "推理声学阶段",
                    cond: "成功 & 不延迟推理声学"
                }]
            }
        },

        "更新声学": {
            on: {
                "完成": [{
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }, {
                    target: "回放就绪",
                    cond: "成功"
                }]
            }
        }
    },

    initial: "推理时长阶段"
});