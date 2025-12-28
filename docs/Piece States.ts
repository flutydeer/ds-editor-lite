import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout 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-6IB-god2CLnjIq1W6qaAigVCOdAofe8gBRuC-MRXZY5Ml6W0i8K4KjRr4BdWQi6WoJUGtwaki0jxpYVCCqSvYtgqgIjnnTDwWqtFA10aWx2eQ7A9iuBcU0JzDDpqatlAZBJc15N1a4z1wLPGoJ8W4YcY4lUhvheeI8HhvVTrmTjGNOLgWJOzRuR4w5cowouDPQSZR7AdVApEis06Fazu7arbNPFNZnFrbUdogsDgcMUXhF4jqFS+kAHtqQZxgQf1XGtwS1DB024cKCyFqPqLRPl4DwZwszETAuBoMkFxUvtSqcHShgSiTPqMob9QbNAFGqWUNq2lMwoqdfhigXciOCNSvi4exxK0HguJPH99pCoMfTE4G9IGKTsaclc7F+NVnWA0A8x4TgdA+SISM60+xqkgUMEiJdrHQM+gg45KIAKYOpVBRXc8RCwU2CtOcMTg4XRjhePYMCEHACMOoAK79zO2yszde5JRHmeBefDYWLgsz6ypYFTDgVRJXx8-5wjPcgtPgsEZMWVxThNyeuwvsq0Pk1McB0IoZwvMBj8zBOCGWPExfsBhECHgig7ii-XWL6hJkJcMFVmr1FLNdus0Ucu4Kq5QsDdiWGQCfbCjKsUfr-nOPpeGzdXjhK+01JJbUDd+xlFjwcDKdoJwluiotlx2NqU8iLRlU4NELQOXDKi5oQqbQopHg6C6YcWr5mAExUwA99EUEALOJgBV6Pq9sMeDQUwWizKEjqrgRniJ5cte7pxiiliFffaBltAcg-B2tzL6IGivFVfIIkbgfJWj8mLU0ygZTN1UEl1FJ1hiQTx2DiHlgHXtmIYYexxEjK7lG1FGtdmkStKx1SbAJAOcE+IzdF0tgyUwwMAZLoXKzJN2IrsYUJJXCtF8L0EQ3AIBwHEP0bjiv0zils4FTcGcpu3ROEtb+qqOjMek2on4qAreZYdO2YDXYM7NF0KKVwi0-Hep+afU7UulTxWCH7kuDwU5B5PCH9iP537eSMhrQyzOnWeldSqZP1CGhFF0PKscX5XrCzuOiR4EzdLf00GJaXsuAdl85ItI0JwvanFYVN71ShurJxeMTko7elRaO7wmJqkMvnnAsl0Z0opL15CdK4Q493DhgUANBegB4vUAED6gAOPTn5sg4hgNM2GdC08PiZ5AsY0FrX9YFAAAqYAQes5+Vp5N0LWFwKYxwD+Hy6YhmHUUKji+aEkc+uQe4RCRIXgWaTo1QwsNSLK-Iuw-+RYf02qkwsBqeuIC8SBAyKBushUxQhsnC7mpouBZ0sBMqPKHmRC7sR4B8tQPkDQKBUUDwvkiBXu3wEkmi-oAYc+6Ui+8cyYzon2Jk+ETeBupMw4beWOQh7OXeV2N0NCfe0oTc1Sm0two2L0-a5QRYpyWON8BB8BxBH0A6WgnsLujosiNchIjMLOGiielsBB6UCBrcpBdhdcugumTCRQzo+QFwAhUClUBADBL4G4hmbgEUhmLCLwSg+C54DOXsER6iOOFsgODBveacH2c81Swo1Ozo909OY4TER4pCs+GhXM0OPYFG7scgSISceEV+x4hmMoR2tKlhi0-IcqOaRCGcbyrgBwY4mYjo9Qewxmx0haie8yBBB4+srgQxugIxwmQaY43IvkZw2kQohwmYkape9RSCgSpQTEe8kUbRdaZQtoRwBg5iMsWOhadRB63aiMfaHQJ4WgOm7BQaUsjc1oPY7Eu0v2cyeRZx8Io6zwyYFkr2ygxQSOiY+uJqxWpoh0kC+GDBjg8GDg5qjg2s2mFkcKK4gECINgS2BB5Rd2mYpojgxQz2u2xWYsNSqccO7EeSJAc+gUCgjoFcoiXYU2OIqecgR8E+S6eSkEvJFouIgk6qY4gs3YU23US0aC8cegVQmqD6nh0JQacpAp9QSpYaASHywSrQyYdGWJTqlYSx+plqhpCpxprgpppKsKvxOg9oVqugRu3gQAA */
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