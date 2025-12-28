import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogBMADgAs2eQE4AzMrkA2ZQEYArMwDssgDQgAnogC0q1Ur3Tle28s2H3hzQF8vZtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBCtnZWxDI1lpQ2lpTWZS5TNLbNVZbGdy+VkHTTLlQx0fPwwcAhJybEAAfUBpzX7iCkBvuUBCpUB6FUAKpUTxVMFhMWSsm007Ur1ZQ1VmZWVZWU1TC0QNBr0DVU09VwdpVS7fEH8+oMHAY2tACnUCFNpoAEI0A+UpLZIrdLrUCbMp2eSqYyGeTyfTKNHVRCvbBOfTqcqGDpuPTdD69QIDVDYf6AmZgxg6JJcXirDIbazSPSafKvTRohTMfnyblY2r1RrMZqtdqdMmfSnBGkA-BAwCMmoBSWIhLLSa0ynOY2Huj25nW0LSqlwQOn0uNuUpOwvusmU8op40GJEAmKmAe+iKIBZxMAq9HalKs6H67I6DHYaSonSqeTMblFREXGqlaT5Qx6A78006BSyN0BD3UwCS3oBQ-UAU4mAPlMKIADZUAFK6AY8imGxlmG9RyEA5sEVbDnpMwozp5G4xTYJfckxVTsY5PJi18QoANrPIVFQdDbzNDuvZsJkCiUaicqgM2dclpq50zpxtTmk+heJyXgVX6-C0XiIahXYP2U0HRDSRVNZ20ZhZFUMVEUMXEClkfEzkA7ZXwId9KEACwjAAbnQBspUAIeVAAdTb920hTt90kawx0zNw2hFDoZxzTQxUAxRlClR8NFKM8INQ-B0IocISGIndf3IzZAOAlE6jAtiEOY0dcXYqMM24ot3gVNC11CFdAFNrIiEhInU2RhCjslyfJClUF5ilkIU9GYoo+wQ0o3CMbNEV49DhjGIIgQWH8yJMzYzx0BoTg6Q5YwUNomKtIpYOMHRH1s2M408rTlTpEFwUM3djIjKxY1ChCjgcBMhzYq9EGzWC9htRE2ILEo1J6Es+Iy2lVXpcEmQ7Pcgs5M9sCAsd4KHc0HAc29nKfIw9ncdLfhVdUtVy0SBuyWNsGFZhp0Aol7m2MUWkzXYimiupE1URby2rOsm1bAL+ojXt+wTQCHBdREpqcpL41Rc8xxu7BV19ANgzWwKCqjRRUp0NpHn5No0yuAp8k0DGXSsjEE2u9T3XwQBGVUAVZsPzCISDJEqHuxyRygM8S8KncPR5DFJKINxMco35eMExa8k2pJ9caAYJ78u7BdjzqNjnmTJM2cRTMkWzFmMQqZxDF4oXQkiWJKb68X-ysG1bOG4dHERZHh1impR22BpmcvTipU1-HBdJzDcMI4SDfDGmVB5WzPAgjoXVuHQ2f0eptDUBd1COJEtY9gSV29-XSOemnzIKecSjKCooKteNAO2jFERNM52iT9cSb0n2M8N0yrBFPRhs8WMhTcAo5EMBWrKNZnVcTLlE7dr5tZTuv06Mv2jfUBKWaONoIJcfQ2aHeoMUA1w0VsZpq+pUYCBJvzFkhzO5+0bBmClSCXnkEfUTZ8pQr0O3DjOJEb+8MfAm1zKurTE1GLWeTchxGgeE4Nw8MTiPHjM-cBo5sz8gQroTopJf7Hw9gAoEDJz6NzhOoGMQE1DbCcMYU49ki4GFvMcB4ZCxwvkwUTbBnVcE9SphfMBcg+zQO2OxBQTw+5K0HioYeGsD7YBJmDIMIC-xNxhjGOMN9g45lcGzPO19URnm0BeBwrtWrj2wZWWsDYWzbl9vIrIKgjRvxRGcJwkECxVWtIrAeKsxHq1HoYwIgAHjUAGbawtNyi3waArIksVDqGcOeR4vcrSxk6EadWJwCTnB-j4ggASyYUzkWJawagdgKAcEcAsrRC7plTEaF0zlmhJiRrxLJOsvzTzymE6wJtDRASSjjK2o4xRlALEksoKJnglCMA0wJnt8L6VyRtY2QE7D7VOA8YcOZHj9NuK3WyCFXhJU8KidJAsviNJTmnWZBUWiGmTMUMRUYNBUPTJdXhF55AFAaouZhJySZnNCVY6w2dCjFBcgXfpaIRELKJIieGIoJnrkAI+2gAyvXrjPP5tQ3DDTqPcTwBwILFA2Ycba6tCRnCKIcjS+ATkBKnuc-25Q+yFEgUjYcoLEmRUeJBVwaTYXaWpb8vJkZhzFRzIWbMhxHjSFBWoAeSIdDuBvsrblFBa4zL5XM9uMZCiJmcMOVwKMEAvFsNtF0dtBEOO5d5TJ-jT40rniKGMZpzg2jkM4cpMhtiKGzFyU4RguTnnNWw7qNqwFEMfAnMhwrTgSvidqwlXIxkdDUEDT5kycGBt6g3Npm0yi4jPGxV56gRS+pZaFNlKTOVnH9ctGYwDVUFXAcaGWSZjCYxcfCD1ZSu6QTuOakx91zFBqyM4Psb9zgnDjPyJwxahnstSRW5NgwAkyIhpwgh7ToxwwRjvZG-TRXX06EKCCSV9g5lfJWQAe2p1lGBegd1gsUYsgrjTwkFm39PvkoY1tw2ivG8Uc7A566yrh+SuzNNhdrDVcDfIcnRQ2uutMcOwDhZUIW9QYfQp6KwXooNhaZyLWmovmWBpZZxUNrNbRUPIvroWxn0QmdDmHBLEFw+tAqBSYxFIqqU+EbMNChV9UYBMiYhTSDowB3WTHqZGw6Wbbpls5DDlZvEo4eREODjYnKlCv9-0UEAG96gB9+MtfEQAXHIAB0QCAHozUzN6s2KGxnyLkqYHivoTO++5HRSgFA0z4rTq5eXAfw10oV7gzj7MqDbRAUZjBGkfCUAsPdHAiaVcTXzlj+XN1uG3Wib8b7-SjbbeDDRosYgUHGFECWRYWIzfhloGLjC5BchJXLMhnF9ijC0Zoj5RxlFfBewAjDqACu-AS5NGMtOYzTVjC5im6BOkdK0rxIuFsSscTwSDus1n66clVfnUuXIaDeFEGI7lcjFK8Uo9gbJsUOK4PGPjesDfK1Zqw1X4y1fUPVoCjWEAnczAtl0u0l4vFW+tz8esHtSa6Rbe4cm+mzbPIsz6T4OgHDeDdtbA3sNAZS3Mm0hGkPEdWReY7jhW7NpyxQoUgOBvfM25jgqZ5YJ8JHI8fNCn0zHFfmceGu1bC0d-rdigVLqeVdS-CBotguRCg6K8rG-TSHX3OESRHrRke-r54i8TXDNhRyNFGAwN8hQlBKA8mQbOGgc8JEji2FPJ6C5RalgLw7jDNFFUmDQhOiGFoQtmI4Ipye89R4l5LQu1VtA1Y7pMhxuZ6sOPbSK7ho9chcq+GIgAG0xIBQQAMdrAiswYVukCCzORxW147oU8QszOFKFmngrJJ9TxMTPjItsbXPKbxM2h4yxaKGKa4mzdqMueNd39Ke0-1+kI3l6RgW8FgTLtI9Z42aPFsfoA4AcpQ-oVCn1cGes+1u7G-XPcCBMFDfi4mqpv6ooiko4V0v8N8ri3w3mnu-bR58P0mKMx07VyHP7cVED8Pk+Nv3v1H0f3-D322gP1RCP3fytFjlxAQiQXdU2RrxPnrx31AK5HAN0HejKHOCsnnwqD7DoXVnghxmQOJnv3TVtybwwJfyokxjwOoTYjCnLXcFjGKDIKALH27GaEUDaAoXUAtFKDXiLkxlLieDYIeFsgH3X2TxQKz1UC4P-AflghFEuiREAiEKN31VKEUDuA0GQwUAOGkIpBTwCXvzQNMmiUwP+nnEMNgxHkNHKEoz3kFH-0H2TzMPr0oLw35SsNoNsPzX6UAnqDZTfjRBUFaBr08Kz2AKDxeiGn8POkCPiQ82vnVjCKcDsjxneBEG4AgDgHEE+BAIUVKHqHBx6ShzC02gOAxQfmcAUH0DmmMLaipGKM2EeDyG0ARiHFlXKAxgnFKT7HKgrwfl0ETH5nJSpAtSCDaP+TflLm6OHGDn6JELyBUQcQeFsFsgeF4imM6lmKzXqA8yWWgyXlg2uH0AMBFCdWGOE2YSmO9B9AOMKhLUgRaCRn0AxknXiVOyRBzCsh1ScEl12KVF7QOPmLei5GOFQRiwnBOFFwfli0cHKjuJ8UAGgvQAeL1AAgfUAA49A4lEQ0InZMFER8P-AY3sRMUcaSSqMoCYikQAAFTABB6wOJll5BKEqhSTqDiRqGNgpPk2pKOFpJumeMkIHlAi0FkgjitH0Q1TRDHU91wOBiPhmLiKznmK6Lk1uBKBFC72KD3WfCiU6DLmBn2NVKNigz3V1V0CcLYOOh4TqM8FuGKAV1RN-U0kGDBLNMsOKgNW5C-UfFjCqJYgaClALFeVPGHmBlBieK9LhFePZW5C5j9O+JqHoggQxg8BzE8DJQJm1meI6NxFlWOF2DHSOyLmKDsGOFREdFzSMBzPdkGGVJJnzPmLQWLKhIUDLNtgxkKTQQqHjGhNdPJX-lNKoLrQnyMCtM5x0I+yXwSjthNjRGbXrKMUGGkRjLHJpkfAgQTI+OTK0NlW3KdBFETAfmK0kU9M3KUJCLsQQicJZm+iLnuF4zLljDqG0D2BXL8UmXzOOELJdI7LqP6VeHqGKGx16PbjQ3nUPh8gCRbJLyLM4nZSApSPhntSOAKH+ntAMTdIpRTVHJ8LmQtMnNHWnNtPiQkjtHaF2lRCUxwvJUaT-TunBNvBHTaCHC5BtAYIqSTAdhtCywxkfAwQyTwoXX8V9GeO3IbReBKgrPPB3V7GgQdFxUgJE0kpdHA0RjkCu0cBWPTCXz3U525BxnGT9360kpsRdVKOHGdDzmOyE0UmdJvjUD2BaBrxIAOMcENCrJsrYgTylGLxuH0BPJdGCMghr1XE8qYJ8q5ILHKCcBZ2qhzGGngL9IeBVmaJwBT2bNjON28rVlivL3UWoQIJUB3na0ig6CiP8Sivyqun2DiqXnxUJKJSTDKBwJ8B8CAA */
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
                "唱法步数更改": ".开始推理唱法",
                "音高步数更改": "推理音高阶段"
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
                "声学步数更改": ".开始推理声学",
                "音高步数更改": "推理音高阶段",
                "唱法步数更改": "推理唱法阶段"
            }
        },

        "等待回放": {
            on: {
                "开始回放": "推理声学阶段",
                "音高参数更改": "推理唱法阶段",
                "表现力参数更改": "推理音高阶段",
                "音素时长更改": "推理音高阶段",
                "音素名称更改": "推理时长阶段",
                "延迟推理声学更改为\"是\"": "推理声学阶段",
                "音高步数更改": "推理音高阶段",
                "唱法步数更改": "推理唱法阶段",
                "片段被移除": "销毁"
            }
        },

        "回放就绪": {
            on: {
                "音素时长更改": "推理音高阶段",
                "音高参数更改": "推理唱法阶段",
                "片段被移除": "销毁",
                "音素名称更改": "推理时长阶段",
                "表现力参数更改": "推理音高阶段",
                "唱法参数更改": "推理声学阶段",
                "声学步数更改": "推理声学阶段",
                "深度更改": "推理声学阶段",
                "音高步数更改": "推理音高阶段",
                "唱法步数更改": "推理唱法阶段"
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