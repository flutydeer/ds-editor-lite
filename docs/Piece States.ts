import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout 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-a8ndqi-JzEoGmFwCQnK2awgBN1T5QqjFcJQCUhxFVOBaVwKYb4hxzC0JwSJ042CfKWQAe2pVnGLKolc83D5UMBi-ewolIP3OSU4VZwMZ8pbvVZJMqqyLgaWy55dQPAqvhZRJwGK5LdM0AUEpZRwoMPilKkssqKBq3NXgzJORQnWhREbe8FwfnnPtA5V1-YnBlAkl6n1fEFGBLuuy6wGg1GPCcDoCSXcUzWn2CU9OhgkRuB7Em01URzGKusUURotjM5dxFjYK05wY1yC7rKF49gnyysAIw6gArv14vIxRQTLWZoUCUdRngtE9ixC4Iczd4myS8M61B38cCDpHWajWdbth0RDlcU4dUBrb0XUVfRqgSgvEkqW-tFZh0rjXAe3c3J7CHnTh4IoiyA5Lu5OopyEsN2PufW+WtkLJ02NFvaRwDizmIEzo4QusEsyuWKGBkdfr91QcDbrKoYSjbXhvVE-9bxHLnj0J-BxJwsN1LBv6p5+HskHHNAdAqhTnCXqbm0dSsYOgugOk+WIgAG0xIBQQAMdogjfR2KhsKiSqC0YORdZk8RdvWl4HMSSt3YDExJ6TJ8A3w1aAUBw+R36Yuslq24BQ+TTljBFNEmcRPiamIZlQFrA1HVivIeWeqzjXDzroA4g5nEOFomXVzi4pMybw-DVQ+wFNhWU4hhAMVW5twxZLOUY8xMxcM48idgbTO+ZaKaZ0g8bMICmse8a9o-MlB0wqfLC5YuME88ZlaxRYq320eErokbbiHGbpnfIJwPBtLtsksT8dDPxcjPeWwRRqYf2UKzK0m87Dbx0L7QwVSZuibmzJor6bLVDkNJJY4rgrj7VaJt56pxWguCqnq1zx2OteZMyhhw8gvCGzOJ3HupklAVtjNac42Z3tg3a4oL73WHK-etEpeo-mhsIDeLYUzkkZTXkKdN3TYnxnzfh+TeT4sO4Vr+U404lyESHB9qmVzxOTuk5-KV8zbT0QeGs04i4ltYIJpzVFvLomWefa6+THrnOIISLOI685JyDg7zfueO9vhegiG4BAOA4h+iS51mUVwjb9rNu509NL3UTj5QOuDl+iXK3VKpAb8+Dpep3qU09ZouhRQfTsLb04TwAakid0qWOwQXciRB3RLQGMvfAR7mncSdFtG0U3Y1H4nkVSR85OoMSugP45gxi8LptQ7jokeMKNutvNBcSpNgH0voc85C0L1FmJwMYMInLGYpoiMyMxeKAkodelT4ubyDtahjzhD26NxgOFh+d5CdK4Q4YrDhPkANBegB4vUAED6gAOPWbzYBQskHhXBsM6J0lv4xvPihoC0DjmsUkAACpgBB62b0bHk3R7-0vqOjx6nZ5Bb9IoH8Zpm9chzhcRDZFYjEnQqVagb02Mn5Whuw9gtAZptJJgwCHg1MoCxoLhYDvom5ihuwnBvlowCcM8tIvIwDclhVe0u5UZYx15agJIGhYD1IHhJIiQvB0Cx9mMEszJJ9aZuxnQBMGILxK9cxFMDpa9qktJLom9+C9l9YjR+NrYSkypbgG0hp-pygpxpFqlHYsCIDh4JZAUCC85JJIIaFKJbwnArhD5w9wYsCQdTDoD8CtAe49gDhHAnpWZ8gLgehsVuZf5qClDG4et35y03BVJy1NsLYDob5Og+xE1DDf5-5FDisplW9VDpQ6oSlhRYFnQ29WISR8QvAKCzoq4+CsjFsGhlAOlUY5Bto85GYDgaFYx6gZQHAv4M9UljCCg3C8CBRPDul6EDgcxBxHQujkQtlw9WEXCcDuDhiLDzkcxuR70oI7s7QjVdMcVUoCAaDNA7B+w-wKi1IWjFdXkUF7B10rhH8uZUk8U5px8mJjZlkaYi1mDEA-Z9ZbxrRTJgJnR08zonjmVMizt8NijdgDBuwlJNATgjpBVOwyjRUr1TRdiFQTUaCUM3oYihx4M6pC0lJLlTM4MERJUx4d0sDijWlBxTRHBig59ahTgFJ7QccaFCldBXMSBm9ZIFBHRbEr4lM0scRsC5A5BHBzwK1osFw+SLRcRYJBltM3E5Bopbx8o4paY9AqhhlRdnDwifx+TFSfZrtP5rlNtjjO4XgGTuxnVMSKQicWF5SBSlTf8bsLTulnE2MuR2TpQTpfAgA */
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
        },

        "更新时长": {
            on: {
                "完成": [{
                    target: "推理音高阶段",
                    cond: "成功"
                }, {
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }]
            }
        },

        "更新音高": {
            on: {
                "完成": [{
                    target: "推理唱法阶段",
                    cond: "成功"
                }, {
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }]
            }
        },
        "更新唱法": {
            on: {
                "完成": [{
                    target: "推理声学阶段",
                    cond: "成功 & 不延迟推理声学"
                }, {
                    target: "等待回放",
                    cond: "成功 & 延迟推理声学"
                }, {
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }]
            }
        },
        "更新声学": {
            on: {
                "完成": [{
                    target: "回放就绪",
                    cond: "成功"
                }, {
                    target: "销毁",
                    cond: "未找到片段"
                }, {
                    target: "程序异常",
                    cond: "失败"
                }]
            }
        }
    },

    initial: "推理时长阶段"
});