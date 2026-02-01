import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogDMAFgBM2AGzyA7NICcatbN3SANCACeiALTSAHNgCs0pc1XW1zDQEZZm6wF8vhtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBFNbDWxVBwtteXt5eQ1DE2zLGztmWQsnFSUNVVcfPwwcAhJybEAAfUBpzR7iCkBvuUBCpUB6FUAKpUTxVMFhMWSs8yVpbDLrC3UXDQsLJQtKxC0ba2tmO2sNJSd5aXbfEH9uoL7AY2tACnUCcYnAAhGgHylebJRbpFagNYlTYeVS7XSuW6yVynBBPbAaaxIzT2VStJSODqvLqBXqobA-P6TYGMVxJLi8JYZVZmeTWJT5J5KFEWWTMHmyDlo8xWWz2BpNEqtZ6dAIjL6-fD-QCMmoBSWNBjLSy0ybOY2BuWMJrhajQqxkQriRmMu9QOgrsFg0xLeZOC2BIgExUwD30RRALOJgFXozUpJkQ3XZVwaWRbRFyZgcooeE4WhBlRSqRzqHkctp8iwu0kKimASW9AKH6gCnEwB8phRAAbKgApXQDHkUw2AtQzrWQgnNgitJpNZVPJmJH3ISRTVxfHmPJDvDp7IC-KPhSYoAG00AjDqAK78KIAY7QBwfB7ahiEemynvbug+xrQFaIxWKRKOYRx2BXcC-eIUAG1nkKioOjNhkQ21Fkj3RBRlB0WwrVcOo+zRY5FEOVwykJBxHA8d9Ai-H9wmieJ9zbEDJDMJRXH1aRVCTKcjg0J8DBTOFMQKCwcSOUiNkwghsMoQALCMABudAGylQAh5UAB1N8JbMFCMhYjslkFRlFUFQhWvK5+yUeD3Exep5EjNM+yfTj8G4ihwhIcTAIPIi1lI8jKMsaiWifVEU1I6NaIUXSyn0-MXldLjv1CT9AFNrMSEgkrVmWktZcnyQppEeNQLAFax4KKbsWJQgoM2sDDfMLIyAoGYYgn+WYCOAqKzD7VwbAOVppCnBo5JKNEilUfIMtcJL5F0FFDO4yklX+WlwqAyLw1MHqapYlwnDkQdaPNKpMxsFj3E0IddmnfrCqpZUaRBelWwqibHmsbAyLk5jB1NJxUsQzqnAcF9VB2xVqQmdVyvGjtJujQVrnsUj8TsDZWq7bYij5DY+WuN6Swrat6ybb6ww7LsezkUinCdDx7vS5CYN0K5KKUeHsC-H1-SDUbLMqiMoxjFEVFuHkVGTKpr3yJQeadeKozkaRydXTcdz3WmpPDE9sDPDxXHxSM7mc5admwOX5fxDN4RYwzAEZVQBVmxwsIzLCizJd+nZFDIxS7lotR0LRZCn0xOTIx5GC5B8uV3gNn8aAYVHDxk+aIMoqCrTqWRHY8RQKMcHKoynWxXryxdfdCSJYlN46ftA0wH3O18rWnAkWNUaPGexFwBU0IUPCF1OfcN3jBNE8yc7RvPZDubsHHsJ14SxbFHaRKwWg0eK+Q2ieU+9wJ05Mz82+zySTotzRYtnAcSjPR27Bqlo9FuHZmmdRv5+bigDZC9vV9zmTTCFc75aUgVCQKady5TdbY+yhP+XZBRPWl8vw3xXhFTuD9NDtXQi4FQdFNBIkdoOKwUZSJ3BRL2BowC+hDAIAbUqcwJZrzznYPIzB6gWHivNWwuhHb2BqtiOSDUjgUQoWTc++Dm6DQ+l9Yh99oTkXuEaNot03D0SqCaQcF1ZCOB5CxNwbRvCcPwOnHh+1AQgn4ZA6EG8dIuDsJ4bWjRHZXEQhoO4HJa6mhwRSPaw1DpmxIQ-M6F16j3FIkUeo8Yv6SJjgaP+3cAE5VsdgA2VNAyBysmYSM0YeqPj7j4u4jtt4yyPi0RwWI1ChLLFWWsjYAIdyDlkbuBpsSURojsJ4Tpo7xQCfHIJScgEqLUSLLcu4on02lrLJ4CsLHsVMVObs-T6jxWQn2QygAHjUAGbafs-wB20cUmQ4EVBhyeBHewcEUw9TaAaJOBxcTHA4XPAgMyjYm06RNZCBx0oKD5rIlhaISjTi2ByJhPMPBOCmbMjOeFwFjR0TEnKhchzFwHsceWTz4T6k+a0eEfY3CyhJIuM5LdhKhUub9EceQdKKSropbSUKGr5BRA0LGhxsTHORe8VFi9l6YrzoTTkJokr1yynZJ5ak1YFCxE6BMyVvk-gNvSxZ0TsgxSym1FCu9tkol-mRCiAsTRCkFZQQAj7aADK9W+EClnVEJBdSwdhFJ7Dapy4lpQOSFAQlSvy+BaUzLAQyh+cl9RJXhMItmQ4nmyJqg1dkBy7hHNVYvR1or6b5zjmrfsbR1CJ0sE8wknJYaeDYUiDiKjaXXwxWGq5yF2pup5fLK0ikzX6gtWYixLrZ7UsCKioqpzpmEKdWsOQ501AmghchRovYE1yHyE4HYbr2Qk1Veohxza2R6LIhPDYWJjHyFLcoJOA4XAgzkqO+xB06ROIEZO6Muw7hUM9UlFQ3rdl+tuFQwNrCN1DUmHwndgLsjSMNJOfsbEanbI2NGRwMJ35UKuA3E5dqfnYFyUjApE6EClOqnyHqtgWJqTPb6-ZV7eY2vynWmZESaaPt1fnRm8SpGs2ahzY8jhTxtAFE5T+-ZR1tLFlB7pjw5Z9KVupbZtgy1JzLgcHqQGa1lhmQQQAZ5GAEXlCgQxAB7apWKDk0OQGm7jsfsl4MxPJ8TGK0xon6YSE9M0TEmvwirw2K-OpSHDUP2NcmEjsjiMPitcMlLg0y6dLMJ-A4mKD8XRdqgF+HsVbBfviiUjwnmExlsiQxRoVCufc550yxBfN0yueUKwhw7kTweU+dTOkbAOFonydQTwBSxf0x5wzmckvm0ZcC2K0ES6KTLmFgc-belxURKVgzIbs0mfDTBDMUaMzy1rtcMjqYHA1WtkUHYtFD6dfK1ffWobesTUsJyWR1wZQ5QQuyWzpFaiDk9g1LQZR5uef9oUu+T7H55D9W-fs93DiO1aOdew9pbi0UsNtc+emuuADe9QA+-ENviIALjkAA6IBABkKhDuTXVzpDg-m8+WXiJHHnC-GAWs6OQs0wjJ0WCWqvOLWNctLU9a60OSrefjNhIxMPhE-HmuPKz46Xj1oppmmUXWOAY2RxwBxLRkF1dqV4nBdXlu5atro8dbgu3JxoBr4S5BQjZBdKYnhlCjYlT7cDHhM-x5V-5yWsW1aLl20uSC1e8nyLThoT4roCal8zrc3njPs763JHFGtJx2CFKetXdRuw41QvcCpeutzCrZ1d-DSlAvYncLYSw6gUrbLcOmSx9kMz8kl6SaXi36VHSjxz+owueYNWxNA64vjjyp-yOngtWew8UAdZHnVpm5LRlsLYNQakrxV9TBea3GCZxNTfOfXPzf8IF9b+Gz7mInTXFXYhlqKf5JtCHwiPko+56581YT3dz7GZ0QyTG+RUc1dDk2GthQ5QGh2WUdvp3FBd8JCn350zlEcXdynEL5KPbz8wQNCOGvyjARAmTH0f1ARbzfz60jSFEUkJE-gngnlvAag71pwrVnXZEbyzT32u2LnzT7luGSjQmVkFxeTIVUj5DQRVXPlXBIEYxzXRnjGUFuC6gyhNRylvBqnvByiOHqBykUnikwjoNGA6VfyN1AhJlWn5BaBgi6mnFV05kUEuEAw9QeAd1JBEMY3kBWyYOF03yoX5HVwA1MVu2xH7DkDuFGWzwCFXC-AYN0MkOtGESJg2yvDRBWh2CtFkR8P5jPjnjsM-EY3EOqxknMJYPEV0AKHcJTD5DbTWlkUuF6h6mEJXHsI6R0Ld3DHCJcLkDcMjDRAnkUAOG8MEKfnvxrVXAIQ6UYMkPZAiKJmaGOHikGXMSsPZGYgFlSOqL3BCKJ0QAHQaKxhKGaMUMtCINqmvQzB6myVoJXB6MYEyML3pialDkOE0DNDKAt0kV5mUH6ScAUHuCSg0NsPmP1kY2kEcJkgUHaiFEsDhFIi2OTyqDTA7022nDYIeROJwFXBmQcKyPRj7CGNkShgeUXXLQ9ianqFSL+LEKuKyE72BNnEK1R1TE8T2WHXcHcCyQw1ONhL3CWOn2yKBNyJBLnHUChQFBliTiYSxF-x8BeBEG4AgDgHEDeABJqwnjqzBXNz70mnUANRRAqS01iUMnJA5Ifg+z2JZkHBfjfhFDh27DkGG2OC0EJH8JrSLHrSCAlOimxGlPZFlIJR5noXIStXT3snuDFPdD2l1LZBeQKB5mRzaAWjBhTHOCRCuCFGLnminGtL6C9G9DtOfV9WEUaDZjTRaGeOPA1wogsNlIsVuEon9IRirGDP1Mxn9RKNaB0hFBuVoR0iKHKHmnkBTOwDaWDN7G7BPDInjF0DND72JQagfCfGIPfkwkAGgvQAeL1AAgfUAA49YMyifUcoS4PnDyBQM-KofOLsfkdaJKRaEoL2GtQAAFTABB62DNokLgonKH2CoUTwVJnKHCogXOOHhmDNMCOICSomnEchYjRCcHakHCFJOwnjTXJjwR1OWImkuAPgvCSkuAHCFEKLUBlgHE0CMViXnBUQGltK-N+kHHany1VJNCnBmNaheQUB7wfPxC0HJnA3TOmhPGxwSmQjkg0jeN5FkSxDjFLOgsKkpiDLgrzly0NHDNdmxyxEKIKANB5nfkzHgOFnXA3ErM2ExjZiSnSzGIQHjkxHWQzBD3uBNFsXPKlMUQsW2AOEwuQQFMrUdDaDJS+RaW4Q-INhUv1LUpO0vS0u-lLy2CdHlm-00D41CVgqJPgocFAsPTcFKDQu-mjWt1di6iFLYlCXCUYrcuYtDMvQ5HYp5k4u-gHAPj0Drm0j5CguAzUXwqYuuKsGqnhBQvZD0D3n2xvwnLW15VxKbj6ArOyqyCrMxmovjCdCjGjIQCRCHBjCsP5BRE9NVRUosUxHF0ss0t222SeHhyo1WS0GuFsFHQ-JmTMu4KGo0rg1avbU5GcGvCJltBsJpVA1cugNOg8qQvsp8oUATTIhtGlEc35AsV2trVAyyoipyrKUjCnAHEcEaDYm9SGQomgmhhRFuFHWw3CsOvgqiq0EsDIjUH0j7270UGNDtCfHiXuobWqqEsrPImxC8WnDmjzU5ReSyS8OxqkW+LAzczK3E3PIUJkV5V7CHBUAKO-ijBqnZCtE2k-krzD2ptKQTzKCShgj5zKFvAFGKPeoKAMUHQqNdBEMrNKTzQ8nhHxCnGSTV0YXMLridE8WhlSK-DlryDzRynKGuWJlRM+plnWRzCauLW6P1n1oum7wUFTyjDuNaLVn6XrMeBVvut+OmXtsNqdpNtds42JUHHZHsClEZwZKAA */
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
                "唱法参数更改": [{
                    target: "推理声学阶段",
                    cond: "延迟声学推理==否"
                }, "等待声学推理触发"],
                "声学步数更改": [{
                    target: "推理声学阶段",
                    cond: "延迟声学推理==否"
                }, "等待声学推理触发"],
                "深度更改": [{
                    target: "推理声学阶段",
                    cond: "延迟声学推理==否"
                }, "等待声学推理触发"],
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