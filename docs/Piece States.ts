import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogDMAFgBM2AGzyA7NICcatbN3SANCACeiALTSAHNgCs0pc1XW1zDQEZZm6wF8vhtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBFNbDWxVBwtteXt5eQ1DE2zLGztmWQsnFSUNVVcfPwwcAhJybEAAfUBpzR7iCkBvuUBCpUB6FUAKpUTxVMFhMWSs8yVpbDLrC3UXDQsLJQtKxC0ba2tmO2sNJSd5aXbfEH9uoL7AY2tACnUCcYnAAhGgHylebJRbpFagNYlTYeVS7XSuW6yVynBBPbAaaxIzT2VStJSODqvLqBXqobA-P6TYGMVxJLi8JYZVZmeTWJT5J5KFEWWTMHmyDlo8xWWz2BpNEqtZ6dAIjL6-fD-QCMmoBSWNBjLSy0ybOY2BuWMJrhajQqxkQriRmMu9QOgrsFg0xLeZOC2BIgExUwD30RRALOJgFXozUpJkQ3XZVwaWRbRFyZgcooeE4WhBlRSqRzqHkctp8iwu0kKimASW9AKH6gCnEwB8phRAAbKgApXQDHkUw2AtQzrWQgnNgitJpNZVPJmJH3ISRTVxfHmPJDvDp7IC-KPhSYoAG00AjDqAK78KIAY7QBwfB7ahiEemynvbug+xrQFaIxWKRKOYRx2BXcC-eIUAG1nkKioOjNhkQ21Fkj3RBRlB0WwrVcOo+zRY5FEOVwykJBxHA8d9Ai-H9wmieJ9zbEDJDMJRXH1aRVCTKcjg0J8DBTOFMQKCwcSOUiNkwghsMoQALCMABudAGylQAh5UAB1N8JbMFCMhYjslkFRlFUFQhWvK5+yUeD3Exep5EjNM+yfTj8G4ihwhIcTAIPIi1lI8jKMsaiWifVEU1I6NaIUXSyn0-MXldLjv1CT9AFNrMSEgkrVmWktZcnyQppEeNQLAFax4KKbsWJQgoM2sDDfMLIyAoGYYgn+WYCOAqKzD7VwbAOVppCnBo5JKNEilUfIMtcJL5F0FFDO4yklX+WlwqAyLw1MHqapYlwnDkQdaPNKpMxsFj3E0IddmnfrCqpZUaRBelWwqibHmsbAyLk5jB1NJxUsQzqnAcF9VB2xVqQmdVyvGjtJujQVrnsUj8TsDZWq7bYij5DY+WuN6Swrat6ybb6ww7LsezkUinCdDx7vS5CYN0K5KKUeHsC-H1-SDUbLMqiMoxjFEVFuHkVGTKpr3yJQeadeKozkaRydXTcdz3WmpPDE9sDPDxXHxSM7mc5admwOX5fxDN4RYwzAEZVQBVmxwsIzLCizJd+nZFDIxS7lotR0LRZCn0xOTIx5GC5B8uV3gNn8aAYVHDxk+aIMoqCrTqWRHY8RQKMcHKoynWxXryxdfdCSJYlN46ftA0wrSSi6h3KDx2aHdSUxHTZ+0cW2tEHWQU+9wJ04ofjhNCwOrLMWQ7m7Bx7CdeEsWxR2kSsFoNHivkNqnpuSTTw3AtE8yc7RvOYqytqULPR27Bqlo9FuHZmmdVOfaXigDZC1fJJO36hXO+WlIFQkCmnVRo-ig1soT-l2QonrS+X4b7ZzvrnGS5hWj5Byi4FQdFNBIkdoOKwUZSJ3BRL2BoQC+hDAIAbUqcwJb3zznYPIzB6gWHivNWwuhHb2BqtiOSDUjgUQoWTc+Lcl6DQ+l9YhEDoTkXuEaNot03D0SqCaQcF1G4cgaMONo3hOH4O4XtYaIJ+Hr0gY8PIOkXB2E8NrRojsriIQ0HcDkmg5IHBwRSNRB06RmxIdo6c3ZjQbG0nyCxX9Y6-17v-ZOtjsAGypoGLu9N86Mx6o+Ae8Z8QV0kQORQ9QPAclaMfNQQSyxVlrI2ACa8g5ZF7gabElEaI7CeE6HxP947+KToA5R+B07YBFluXc4Spa9hlo8OWCtzHsRMVObs-T6jxWQn2QygAHjUAGbafs-wB00YUmQ4EVBhyeBHewcEUw9TaAaJOBxcTHA4c3AgMyjYmw6b9Kemw5xPTcI0GEaIeo1EsAcZCDR4xsymbMjOeEwERS0WsAu+oyLIQFmXdwTzSJWAaiUMOZQBwOG+T+duK9-ljUBWYK01xlDy0OPcIc-ZbhPMuOdJKLF1lqEFMche7wznL07os7u2RGj6gTDoKMit2RPMsNGI4tdG6jKjMiygBs0WXI3poWKs4kmlAkceFEviyIUQFiaIUIqKCAEfbQAZXq3wBUs6ohILqWDsIpPYbUSUNWUEnPERwig0r8vgelFAZmgIlZAuS+okrwmEWzIcTzG41VhbcKhdwjkapAYypxAisVDmmv2PMjgGq3HkAGqeP8KIawoXHDV18o0FOZaYRSihBzwn5LYIcdwObHl7JsY4bg5JeJoiKoqpzpmEPdWsOQ501AmmOFaacth5Wpg2NGRw7JDgOHZCTFt9jAQaOjZi7IOithkSnhsLERjU3bIrda9kiLWhTzkrOoaDijrgKXZNEomI+zuXUFGJwlwA27ODQcsNRwT28I1EyiJ0jDSTgTbzJax5R0wJhG-KhVwhaNPpdgbJSM8mdsQMU6qfIeq2BYmpZ9Qb9mht5g6-KsGZmhJpoug1kTozRKkazZq1bUxJplm0AUTkP79hba0sWSHUxdNlk8PpSsEnHl3aUHY8sDjPMwmWGZBBABnkYAReUKBDEAHtqlYuOTQ5AaXuOx+yXgzE8uJMYrTGkfpJ0s0n8DyZMp+cVP7Tp93cErcRuxNA9Uds52og5RlIjdvPV0UnpmyYU6i-NF7yPYrrXi58hLFJPMJjLZEBijQs1M+ZyzpliB6oxeR65Ww+R3K6k0YdnmQVrJRCaJJ0Hm7+cCyZTOmW6YTStPiNW9VLx2h2LFp03ZFbUVLSPFLAWLMKcAG96gB9+LbfEQAXHIAB0QCAHozWbamCsy3lja0pRRrixZ0vF8FngebsgGzVyN9XzZ51BfGstuwK23GVseAzU1yvuFtAKQ7Q2r76zdbZh+cl8iXgJWRHmfG3PqA8xQuWV5CSvcs-7fJoXC0oanK-fsSPDiO3Sda+0txaKWG2pwlTot0snecWsHLtyFoFceSmPjVghSJUVopdwvnST463F+GzZHC2spsAhSinLzHcqp8hRQtPh7XDgZV2l2AWe-n-Gpxoxr4S5BQjZbdVQnhlDVk4UXDULwS9dNL3CWcltdRBcXcF04hxRyp32OtOMkTlAzE8TC0vgtE5jRGZVuLtb-aJcB9E5Rzo+qJolJKBGAjS7FSF-Vha+ztXccOZE6grdVHKG4Va7Fri1vnHjysotXVR6y4WmEtR4pXBaJRYeRX10y2OPiVo47SLO9z1uHVbvL3jwNJGK4FCBQDgHClbZ5jGFHBNGhdXwqc8E+CgXhrv1zvdgTQ0JN8YtC3hXbTlijgXBChe5Prcea2-keLVsQo5aGpuzow1DYez7ZX-ZChTCq4SCca+6BVSygbtITUCwnKt4ar3hyiOHqBykUnikfxXGf3aXPWj3phJlWn5BaBgi6mnFVzOEUFJUBlZgeD11JCf1GHaXkA51gIcHgK6jjHV0gxMTyGqn7DkDuFGSZwCFXC-BfyIPDGxHOmESJkFSvDRBWlE0Z0bhjnMXAJYKgLYPRmtC4LkB4MjFaiFFyytFkRyl0B6lEM-E40IILVgKkJuxkIKF4JTCnkUDeUZ1HVJXAIIXaVfxkkfQ-3EWal5nikGTMXoP3SfAFksP1k42gML1gPZHsKJmaGOGcMrluDZX6VrwHAUEYJwFXCsL3C0Lh3pialDkOE0DNDKCQUrl5mUH6ScAUHuCShwKYJXASMYGkAkNAhiM115QolIiyIHxTzKGjCgy0BYkOEbl7HAJmVYO0PYL7ECLkFnD5HUEtX1BE2xCwVhmz2blXF6PEP6PRkGOkMbihi6KhS6hv0uHcAcyaB6PbQIKqNsJWL0LWLnDGO2QKE5AoWnT2OSmgxeBEG4AgDgHEDeCWLOzKCsFBRLjsAtxNBFG-2NQ8m9XFAmUaXJE+MgSxzyJZkHGflfhFGWyoVBPtz7GnDD3eHJFbSCGhOimxDhPvyHEUiRJyPIUKBaHuF7FDyUROSXB4XwHxLZFcWuOBl2CdjuGHXOCRCuCFAHXminEMhxK9G9GZOXSDWEUaDZiRB5ixCeQ1woloIRPMVuEomFPdHg3FMJMxnZHMRYlaB0hFAOFqFBLKCPTKA1L6FaXFK6UxjInjF0DNE-ip3IkJX5FpN71iOwEAGgvQAeL1AAgfUAA49cUyifUAPBMSiHSBQZPLFLsfkdaJKRaEoL2SXQAAFTABB63FNokDwonKH2CoUsBdKqHznjMt3smTOOHhnFNMCKJ-iokxNohYjRC1xP15CXwSixHJjwTxOSImh2KJKSkuGiKaLQPainU0EMUjF0HJj2hrNLUYyrTcFKB6lQIQChjVn3XuC1zr3kHJi1L7MkO7BPA5BUDGR6kEwQFchsHqC6kbixDjD3MaQGkpjFMPLzm20NGlNdlPPlKMIKANB5jfkzEUixKwkKhtPfODk2ExjZnJTQz4MuBvSa0UjUmxDApUXIBrNhLcF3L1LQ1HIQGQhB3MVUN0A3WwUaWaR7INmwsJNwv53woUEIr7RuSdFWx0k0HEyCTnKgsEXHPiXYpXIUBMQzHyBHALhRB9QwqaW4RCTfJgNOklJDTkWxjlJYoHAPj0CFH5AUD5FmMl0woRirHFIaBKTxVH3ZD0D3lIi2CjBjMsBaBPiCUgsUo7DtJPAfPjCdAfUGRqh6noN0qtEuBFWwvMUxDE3rhDWYp5WtAcGgkaDK0ovpNgx7JmTov-0iu2AOBiquJNC2AKGvCJltG9LbXeiZL4r1AEqXMstXM2P1CxGlGuF0H2FKqdR+Tg0RlMphSvCnD720zYgDSGUzUkvdluBbWIwUr8KUsApTUsDIm-xJieXHQUkuibOiTatg1cumvcvImxA20HUeGIpJVcUatE32qkRKJwGqyG3nLSj2MLPLh0j92nP8uxFBShgKAaWbhZ3nOKSHW+KHEdCSVvAFBML6oKH0R2EaHAJIFtOKWIo8nhHxCnDuD-wuAfEAKVk6LAuYM-HhryGIpynKDBWJmHUcCsGuGCtQgEO9PiP1gJoujUGJrcBLiFGHSRCGV7gwQaEeFRrppXHSsqrAkJuZoUFZvstsHGJP3ZHsClB5h8B8CAA */
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