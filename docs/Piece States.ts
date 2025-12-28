import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogBMADgAs2eQE4AzMrkA2ZQEYArMwDssgDQgAnogC0q1Ur3Tle28s2H3hzQF8vZtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBCtnZWxDI1lpQ2lpTWZS5TNLbNVZbGdy+VkHTTLlQx0fPwwcAhJybEAAfUBpzX7iCkBvuUBCpUB6FUAKpUTxVMFhMWSsm007Ur1ZQ1VmZWVZWU1TC0QNBr0DVU09VwdpVS7fEH8+oMHAY2tACnUCFNpoAEI0A+UpLZIrdLrUCbMp2eSqYyGeTyfTKNHVRCvbBOfTqcqGDpuPTdD69QIDVDYf6AmZgxg6JJcXirDIbazSPSafKvTRohTMfnyblY2r1RrMZqtdqdMmfSnBGkA-BAwCMmoBSWIhLLSa0ynOY2Huj25nW0LSqlwQOn0uNuUpOwvusmU8op40GJEAmKmAe+iKIBZxMAq9HalKs6H67I6DHYaSonSqeTMblFREXGqlaT5Qx6A78006BSyN0BD3UwCS3oBQ-UAU4mAPlMKIADZUAFK6AY8imGxlmG9RyEA5sEVbDnpMwozp5G4xTYJfckxVTsY5PJi18QoANrPIVFQdDbzNDuvZsJkCiUaicqgM2dclpq50zpxtTmk+heJyXgVX6-C0XiIahXYP2U0HRDSRVNZ20ZhZFUMVEUMXEClkfEzkA7ZXwId9KEACwjAAbnQBspUAIeVAAdTb920hTt90kawx0zNw2hFDoZxzTQxUAxRlClR8NFKM8INQ-B0IocISGIndf3IzZAOAlE6jAtiEOY0dcXYqMM24ot3gVNC11CFdAFNrIiEhInU2RhCjslyfJClUF5ilkIU9GYoo+wQ0o3CMbNEV49DhjGIIgQWH8yJMzYzx0BoTg6Q5YwUNomKtIpYOMHRH1s2M408rTlTpEFwUM3djIjKxY1ChCjgcBMhzYq9EGzWC9htRE2ILEo1J6Es+Iy2lVXpcEmQ7Pcgs5M9sCAsd4KHc0HAc29nKfIw9ncdLfhVdUtVy0SBuyWNsGFZhp0Aol7m2MUWkzXYimiupE1URby2rOsm1bAL+ojXt+wTQCHBdREpqcpL41Rc8xxu7BV19ANgzWwKCqjRRUp0NpHn5No0yuAp8k0DGXSsjEE2u9T3XwQBGVUAVZsPzCISDJEqHuxyRygM8S8KncPR5DFJKINxMco35eMExa8k2pJ9caAYJ78u7BdjzqNjnmTJM2cRTMkWzFmMQqZxDF4oXQkiWJKb68X-ysG1bOG4dHERZHh1impR22BpmcvTipU1-HBdJzDcMI4SDfDGmVB5WzPAgjoXVuHQ2f0eptDUBd1COJEtY9gSV29-XSOemnzIKecSjKCooKteNAO2jFERNM52iT9cSb0n2M8N0yrBFPRhs8WMhTcAo5EMBWrKNZnVcTLlE7dr5tZTuv06Mv2jfUBKWaONoIJcfQ2aHeoMUA1w0VsZpq+pUYCBJvzFkhzO5+0bBmClSCXnkEfUTZ8pQr0O3DjOJEb+8MfAm1zKurTE1GLWeTchxGgeE4Nw8MTiPHjM-cBo5sz8gQroTopJf7Hw9gAoEDJz6NzhOoGMQE1DbCcMYU49ki4GFvMcB4ZCxwvkwUTbBnVcE9SphfMBcg+zQO2OxBQTw+5K0HioYeGsD7YBJmDIMIC-xNxhjGOMN9g45lcGzPO19URnm0BeBwrtWrj2wZWWsDYWzbl9vIrIKgjRvxRGcJwkECxVWtIrAeKsxHq1HoYwIgAHjUAGbawtNyi3waArIksVDqGcOeR4vcrSxk6EadWJwCTnB-j4ggASyYUzkWJawagdgKAcEcAsrRC7plTEaF0zlmhJiRrxLJOsvzTzymE6wJtDRASSjjK2o4xRlALEksoKJnglCMA0wJnt8L6VyRtY2QE7D7VOA8YcOZHj9NuK3WyCFXhJU8KidJAsviNJTmnWZBUWiGmTMUMRUYNBUPTJdXhF55AFAaouZhJySZnNCVY6w2dCjFBcgXfpaIRELKJIieGIoJnrkAI+2gAyvXrjPP5tQ3DDTqPcTwBwILFA2Ycba6tCRnCKIcjS+ATkBKnuc-25Q+yFEgUjYcoLEmRUeJBVwaTYXaWpb8vJkZhzFRzIWbMhxHjSFBWoAeSIdDuBvsrblFBa4zL5XM9uMZCiJmcMOVwKMEAvFsNtF0dtBEOO5d5TJ-jT40rniKGMZpzg2jkM4cpMhtiKGzFyU4RguTnnNWw7qNqwFEMfAnMhwrTgSvidqwlXIxkdDUEDT5kycGBt6g3Npm0yi4jPGxV56gRS+pZaFNlKTOVnH9ctGYwDVUFXAcaGWSZjCYxcfCD1ZSu6QTuOakx91zFBqyM4Psb9zgnDjPyJwxahnstSRW5NgwAkyIhpwgh7ToxwwRjvZG-TRXX06EKCCSV9g5lfJWQAe2p1lGBegdnJIL5ADiOKUKhub9KKryc4JRtgFFsNIU9FYL2nJVSuzNhVXDDQfboOoBwNCsyLvsOwzghxSleG-VwBijnYHPXWbC0zkWtNRfM3a21ZXLIMPoTw-S-rX0eJbKJGMuR-oA4JYgeH1oFQKTGIpFVSnwn6TfTpIyEw2jcFxRjdZPx6xvZGWVeQVAHCHAYFQexKMuj7Hc2cQ4iS3DExQQAb3qAH34y18RABccgAHRAIAejNzNScKltbGfIuSpgeK+hMSgXRcg6KUAoKFf5YcnkByx-LjaCuHcYZooqky6D4zmJRwnOgt2-jp5VrHqZGwOfkBTKygIY1eHE228GGhWRvlC1DbgdMiwsRmgjNjIqdxzHV04bMOit3KI6R4bE6hyFfBewAjDqACu-AS5MWMtLYzTDjC5im6BOkdK0uX6iFsSscTwSDus1n64BlLXDNiXIaDeFEGI7lcjFK8Uo9gbIdaXnjHxvWBsVZsy0DFxhcguQklGmoJ3MwLZdLtJeLxVvrYk5t1d0nTZdItvcOQw5YPvbPIsz6T4OgHDeNdtbA2cM-OAwRm0RGllnDI2slx2NW7Nv+jZWyZKKQ3aVcTDHgW5lnlgnwkcNGDjQ5kMcV+Zx4a7VsAmf7A2qUBaq0F+EhWrIGF0a8rG-TSHXw-Utz1gF+cUERUDkDUcjRRgMPxkTJQHns90A0LnhIkcW2V6uXlmOgtdKFWF49YqNDHefGdhC2YjgiiFMr5LI3UtN3VZpwU2ruZ6sOPbSK7hQ9chcq+GIgAG0xIBQQAMdrAikwYFrcC7zFA-izY7oU8QszOFKFmngrIx-jxMZPjIrcbXPEbxM2h4wFjkG9q4mZNm7UZc8K7GG48J8r9IavL0jB14LAmXaR6zxs0eLY8jCZXDIfQwqOPq4k8p9rd2N+6fIOogKKhsUNUjf1RRFJRwrpf7L5XKvqvdOXq2kgf9V5SYozHTtXII-txUQPw+T4i-V+B8343zvzgQTEfz3ytFjlxAQiQXdU2TLxPkr3X3-AcC33+hinOCsinwqD7DoXVnghxjgOJiv3TRRX5WQO2mAKokxgwOoTYjCnLXcFjGKAIL-0H27GaEUDaAoXUAtFKDXiLkxlLieEYIeFsm7yX1j3gJT1UFYP-AflghFEuiREAl4P131VKEUDuA0AQlOHzTEIpDjwCSv0QNMmiXIO33nAUAOHxUNHKF9V5nYKlDL0MMr2IPw1IKGnvwTAsPzX6UAnqDZTfjRCUzKCcKtX7xkJMI8IoO8KsPiW82vnVkCKcDsjxneBEG4AgDgHEE+AAKNiSh4TBx6Uh3hgnCzwxQfhaExSRAcF4ipFyKbna1LgRiHFlXKAxgnFKT7HKmHGWXjVqKVCPiCHqM2HDiaKj2HGDnaP4LyBUQcQeFsHJwwQyW+GpE6mGM5B4W8yWU6AqhmxqGuH0EUzfiSm6N-WYSpGwG9B9HWM2hLUgRaCRn0AxknXiVO2qPkx1ScA6G-ww1LEwzuhuLfj7ANQ8xOCjBKAjitCsBOEKwfib0cHKjOJ8UAGgvQAeL1AAgfUAA49G4lEQ0RwW4D9OEr-Do3sRMUcaSSqMofmBUQAAFTABB6xuJll5BKEqhSSgxJM+yhwpKOCpJuhuKsBEIHlAi0FkkhJqH0Q1QfkFSRFcH0GBkGJIH5NGO0CtluBKBFDFA0Fgh9XUCiU6DLmBjWOFzmU0z3V1V0FsMYOOh4QfkYluGKCJA0GBl7UBOKgNW5DaCsiSjHHkg0KlALFeVPGHmBlBmuONLrTuPZW5C5g9JeP2LRkOi7hqk8Ap3dnICVOOFxBk04nZVtPXgOFxDLkTBZLUWpIJn-kGJJiVKBLQWOF2DHSO34PtjkDQQqHjGOCKEkSNJIJNOHyMHNO53UJb2tBzASjthNjRGbVTKMUGGkTDJ7IjIgSjMeNjNUNlUfG2m0RFETAqNREkRdPDLYP8LsQQlsJZm+iLnuFCgti-zqG0D2GnL8UmQzLyFrJzIbNUJeEGWKGy2QSgV2nNUGICWrLz2zPrIUEbPTDNHtSOAKH+ntEXwJkaQAX5NNP7NHUHKtPiQkjtHaF2lRCOA6B7QBMPKQNvBHTaCHC5BtGoIqSTAdhtDfk7kfCWN+IpRTUXXnLcJNMjI0DqCAiz3PB3V7GgQdFxR3zE1QrpiU0gkOHhg4iazRBjGOJ6J7l2kQoCBu1QpsRdVKFsmLn2FKGOyFEzDYgdJvjUD2BaDL0VNItMkcENGODJPjnGLZwQBxHDhZgL1lNOD0ICAvxuIcsLKun2ALFaxzytAPwLEYoxgeBVj8pwDjyrLsvCVoKct2lCsL3UWoSwJUB3maBeAqCIvP1j2ApSvZ0crVigzCqXmsI1S5HKBlAxh8B8CAA */
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