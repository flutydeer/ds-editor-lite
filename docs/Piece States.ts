import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogDMAFgBM2AGzyA7NICcatbN3SANCACeiALTSAHNgCs0pc1XW1zDQEZZm6wF8vhtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBFNbDWxVBwtteXt5eQ1DE2zLGztmWQsnFSUNVVcfPwwcAhJybEAAfUBpzR7iCkBvuUBCpUB6FUAKpUTxVMFhMWSs8yVpbDLrC3UXDQsLJQtKxC0ba2tmO2sNJSd5aXbfEH9uoL7AY2tACnUCcYnAAhGgHylebJRbpFagNYlTYeVS7XSuW6yVynBBPbAaaxIzT2VStJSODqvLqBXqobA-P6TYGMVxJLi8JYZVZmeTWJT5J5KFEWWTMHmyDlo8xWWz2BpNEqtZ6dAIjL6-fD-QCMmoBSWNBjLSy0ybOY2BuWMJrhajQqxkQriRmMu9QOgrsFg0xLeZOC2BIgExUwD30RRALOJgFXozUpJkQ3XZVwaWRbRFyZgcooeE4WhBlRSqRzqHkctp8iwu0kKimASW9AKH6gCnEwB8phRAAbKgApXQDHkUw2AtQzrWQgnNgitJpNZVPJmJH3ISRTVxfHmPJDvDp7IC-KPhSYoAG00AjDqAK78KIAY7QBwfB7ahiEemynvbug+xrQFaIxWKRKOYRx2BXcC-eIUAG1nkKioOjNhkQ21Fkj3RBRlB0WwrVcOo+zRY5FEOVwykJBxHA8d9Ai-H9wmieJ9zbEDJDMJRXH1aRVCTKcjg0J8DBTOFMQKCwcSOUiNkwghsMoQALCMABudAGylQAh5UAB1N8JbMFCMhYjslkFRlFUFQhWvK5+yUeD3Exep5EjNM+yfTj8G4ihwhIcTAIPIi1lI8jKMsaiWifVEU1I6NaIUXSyn0-MXldLjv1CT9AFNrMSEgkrVmWktZcnyQppEeNQLAFax4KKbsWJQgoM2sDDfMLIyAoGYYgn+WYCOAqKzD7VwbAOVppCnBo5JKNEilUfIMtcJL5F0FFDO4yklX+WlwqAyLw1MHqapYlwnDkQdaPNKpMxsFj3E0IddmnfrCqpZUaRBelWwqibHmsbAyLk5jB1NJxUsQzqnAcF9VB2xVqQmdVyvGjtJujQVrnsUj8TsDZWq7bYij5DY+WuN6Swrat6ybb6ww7LsezkUinCdDx7vS5CYN0K5KKUeHsC-H1-SDUbLMqiMoxjFEVFuHkVGTKpr3yJQeadeKozkaRydXTcdz3WmpPDE9sDPDxXHxSM7mc5admwOX5fxDN4RYwzAEZVQBVmxwsIzLCizJd+nZFDIxS7lotR0LRZCn0xOTIx5GC5B8uV3gNn8aAYVHDxk+aIMoqCrTqWRHY8RQKMcHKoynWxXryxdfdCSJYlN46ftA0wrSSi6h3KDx2aHdSUxHTZ+0cW2tEHWQU+9wJ04ofjhNCwOrLMWQ7m7Bx7CdeEsWxR2kSsFoNHivkNqnpuSTTw3AtE8yc7RvOYqytqULPR27Bqlo9FuHZmmdVOfaXigDZC1fJJO36hXO+WlIFQkCmnVRo-ig1soT-l2QonrS+X4b7ZzvrnGS5hWj5Byi4FQdFNBIkdoOKwUZSJ3BRL2BoQC+hDAIAbUqcwJb3zznYPIzB6gWHivNWwuhHb2BqtiOSDUjgUQoWTc+Lcl6DQ+l9YhEDoTkXuEaNot03D0SqCaQcF1G4cgaMONo3hOH4O4XtYaIJ+Hr0gY8PIOkXB2E8NrRojsriIQ0HcDkmg5IHBwRSNRB06RmxIdovsRc5Ici6g3eMn9K4xx-vHXu-8cq2OwAbKmgYu703zozHqj4B7eLuI7Acih6geA5K0Y+agQllirLWRsAE15ByyL3A02JKI0R2E8J0X9Y6-0CUnQByj8Dp2wCLLcu5IlS17DLR4csFbmPYiYqc3YBn1HishPshlAAPGoAM20-Z-gDpoopMhwIqDDk8CO9g4Iph6m0A0ScDi4mOBw5uBBZlGxNp036U9Nhziem4RoMI0Q9RqJYA4yEGjxjZtMuZGc8JgIilotYBd9RkWQgLMu7hnmkSsA1EoYcygDgcD8n87cV4ArGkCswVprjKHloce4Q5+y3GeZcc6SUWIbLUIKE5C93jnOXp3JZ3dsiNH1AmHQUZFbsmeZYaMRxa6NzGVGFFlADboquRvTQsVZzJNKBI48KJalkQogLE0QpRUUEAI+2gAyvVvoC5Z1RCQXUsHYRSew2qkoasoJOeIjhFFpX5fADKKCzNAZKyBcl9RJXhMItmQ5nmNxqnC24VC7jHM1SAplTiBHYqHNNfseZHANVuPIQNU8f4UQ1hQuOmrr7RsKSy0wilFCDnhPyWwQ47gc2PL2TYxw3ByT5PaM+pznW-KKmcmZhCPVrDkOdNQJpjhWmnLYBVqYNjRkcOyQ4Dh2Qk1FTw-agINExqxdkHRWwyJTw2FiIxaadmVpteyJFrQp5yUXfYldjjC1RN6d2fExw7BeqSioQNeyQ2HPDawy9Q1Jh8LXYayaQjbiTkTbzJax5J0wJhG-KhVwhZNIZdgHJSN8m9sQCU6qfIeq2BYmpd9waDlht5o6-KyHZnhJpoBotkZoyxKkazZqNbUzJplm0AUTkP79kXW0sWGHUzdNlk8fpSsK5VAAfqUoOx5YHBeZhMssyCCADPIwAi8oUCGIAPbVKwCcmhyA0vcdj9kvBmZ53iYxWmNI-BTpYlP4DUyZT8ErmV3r7u4JW4jdiaB6o7LztRBxjKRG7eerpFMzJU+ptFBbwHrvziqvF2tCVIkUs8wmMtkQGKNCzGzdmHOmWIPqzFQGblbD5PcrqTRx0BdBeslEJpkmIebmFiLJlM6FbphNK0+I1b1UvHaHYqWnTdkVtRMtI8cvhfs+pqN7XzZ5zBQm8tuxK23GVseczU16vuFtAKCbLX82zecWsXQnIkUciuGRHmInfPqH8xQuWV5CR7am7+f8umsNTlfv2L7hxHYZJtfaW4tFLDbU4c1l7gA3vUAPvxXb4iAC45AAOiAQAZCpI90xVmW8tbVlKKNcVLOl0sQs8DzdkmFtOi3y4d2N2QSt3IWhVp5KYRNWCFIlRWil3AhdJOTrcX5nM0aiWymwCFKJcvMTypnyFFCs+HtcOBjW6XYB569xZAuJqNBNfCXIKEbIHqqE8MoasnCy4aheBXrple4SzujrqoLi4QunEOKOTO+z1pxkicoGYnhk8rKLKLVPYs4vrfi58RLa63nKOdX1RNEqvp96LcV0WDVFr7O1Y06CkRRnUM7iT5jGFHBNNcOt85OHK7dUnorRaYS1HilcFolFh5VZ3TLY4+JWjTtIvHrcuqA9AfHgaSMVwKECgHAOFKOy8+rXYmhA3IrS++958FCvHXfoLe7Imhoyb4xaAj9K1nLFHAuCFLt+fCf9bupcxNEtWxCgVoam7FjDUNj7Ptk-9kKFMKrhIPxy-6N4zKFWyQjUBYRylvBqnvByiOHqBykUnik-xXG-w6SOhi0NRJlWn5BaBgk8SKDRHODJUBlZgeHN1JC-1GA6XkDVz-3ah2F5DjAN3gxMTyGqn7DkDuDGS5wCFXC-B-0oNAmxHOmESJiFSvDRBWhk050bhjnMXgO4KQN4Jkn4IAPEV0AKBEJTDK1KytFkRyl0B6hkM-H4woNvXDEUMELkGEMjFwO-neU50nTJXgIIQ6V-z4PZCUKJmaCfT10tFMRGTYJPSfAFgcP1n42QOT3picAENWyxhKE8MYPZQGVbwHAUA4JwFXEcL3CMJQJZSalDkOE0DNDKCQUrl5mUAGScAUHuCSmIM4JXHSMYGkHkOKTUCNz5QolIkKPHwkzKGjAQy0BYkOEbl7HgNmR4OMPRlcTMMbihkGKtSkyTmxCwVhhL2blXBGLkLGL4ImKiKmLnHUGhS6hf0uHcHcyaGGO7XIMaMQFsEiOUNnD5D2J2QKE5AoXnROOSkQxeBEG4AgDgHEDeA2MgSlysDBRLjsEdxNBFGAJNUwW3XjDKUMnJABOinMVKJZkHGflfhFAxyoUfDwyoWQhUARPdDwSCCRLMCONRPfyHEUkxOKPIUKBaHuF7FfSUTbXJCXTJI3WnFih5nxTaAWjBhTHOCRCuCFBHXminCJL6C9G9E5MmmDWEUaDZiRB5ixGeUNwohYPRPMVuEoilIRirE5OxG7BPHZHMRYlaB0hFAOFqA8iKHKHmnkH1NaXXA3E5O6UxjInjF0DNB8X13IiJX5BZJHxSOwEAGgvQAeL1AAgfUAA49TkyifUSPBMSiHSBQHPbFLsfkdaJKRaEoL2RXQAAFTABB605NoijwonKH2CoUsD9IzOlyd3slzOOHhjlMqJ-iomnEchYjRGNxvxRFAyngW3JhJJIDlIpMZMd0uCSM6LOGaLnU0EMTo2WMV38nenwDlLLXY2rTcFKB6i8IQChjVhPXuGNzbydKaQGlQyNOmhPA5BUHGR6nE0QFchsHqC6kbixDjHPLbQGkpllKyLvQVNDTkWxlVJnIQC5lBjfkzEUjI0XAGjaXdM2ExjZgpRw1EMuExA2QzHuEUmxDgovnIDHJRLcDPLNJw3AuQlu3MQ8HtHuMHxCRJINjHONNIvF3IoUEoqu1K1IqnBgnNO-JXOaVUSVA3IcC3IbUL26P3OS3ajaFdnfPxR5hCTCX-LCNOiAoOBApVJaEooHAPj0CFH5AUD5GXKdRaSvIAvDAaFKXxSkp0PHRglIi2CjDTMsF0qOBCUQqso7A9JPE-PjCdCjEoouxjDYOMqtEuFFWIryDYvrlDU4t5WtAcG3WpV0kXRJNmRYvANk3ioOESseJNC2AKGvCJltFDK7TXLEvagcG3Kkr3P2P1CxGlGuF0H2AqvbT6EsvUo7BsuqjtlHyMzYkDWGSzQLjkn7NbSEooxmR9A3M0seBmmAJJmeWnQUkulokTEbl41dPdPImxFx1HUeCotJW5OapkwOqkWqJwHBzUw3LShOOrPLh0kgwQDoxqnZBxWwIKEaWbh5w3JKTHTKCSicq2n3Lfy0ntgoSnh2EaHgNHJ8tAlYIuh0B0nhHxCnESUlwuAfEgKVgGIIpdK-HdJKSopynKHBWJnHUcCsGuEitQnENDLSP1hJryDJoUDcBLiFEcp8N7gwQaEeExqZpXCysRuDlJrUHJs5tctsFmJv3ZHsClGUp8C8CAA */
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
                            target: "更新就绪",
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
                        "占用解除": "更新就绪"
                    }
                },

                "更新就绪": {
                    type: "final",

                    on: {
                        "完成": "#片段状态.更新时长"
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
                            target: "更新就绪",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "音高错误"
                    },

                    exit: "销毁任务实例"
                },

                "等待释放": {
                    on: {
                        "占用解除": "更新就绪"
                    }
                },

                "音高错误": {
                    on: {
                        "重试": "开始推理时长"
                    }
                },

                "更新就绪": {
                    type: "final",

                    on: {
                        "完成": "#片段状态.更新音高"
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
                            target: "更新就绪",
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
                        "占用解除": "更新就绪"
                    }
                },

                "更新就绪": {
                    type: "final",

                    on: {
                        "完成": "#片段状态.更新唱法"
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
                            target: "更新就绪",
                            cond: "文档模型未占用"
                        }, "等待释放"],

                        "任务失败": "声学错误"
                    }
                },

                "等待释放": {
                    on: {
                        "占用解除": "更新就绪"
                    }
                },

                "声学错误": {
                    on: {
                        "重试": "开始推理声学"
                    }
                },

                "更新就绪": {
                    type: "final",

                    on: {
                        "完成": "#片段状态.更新声学"
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

        "等待声学推理触发": {
            on: {
                "开始回放": "推理声学阶段",
                "音高参数更改": "推理唱法阶段",
                "表现力参数更改": "推理音高阶段",
                "音素时长更改": "推理音高阶段",
                "音素名称更改": "推理时长阶段",
                "音高步数更改": "推理音高阶段",
                "唱法步数更改": "推理唱法阶段",
                "片段被移除": "销毁",
                "延迟推理声学更改为\"否\"": "推理声学阶段"
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
                    target: "等待声学推理触发",
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