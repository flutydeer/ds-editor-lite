import { createMachine, assign } from 'xstate';

interface Context {
    inferDurTask: any;
    inferDurTaskInput: any;
}

const pieceMachine = createMachine<Context>({
    /** @xstate-layout N4IgpgJg5mDOIC5SHEnQrdaDanQgAYDpAVxoMBdA300H9UwNjTUBiNQaojBvz0BM0gbQAYBdRUABwHtYBLAC59uAOw4gAHogDMAFgBM2AGzyA7NICcatbN3SANCACeiALTSAHNgCs0pc1XW1zDQEZZm6wF8vhtFjwiMkpAZ-TAAl9AWBVAB89AF7NATlMWdiQQHn4hUXEpBFNbDWxVBwtteXt5eQ1DE2zLGztmWQsnFSUNVVcfPwwcAhJybEAAfUBpzR7iCkBvuUBCpUB6FUAKpUTxVMFhMWSs8yVpbDLrC3UXDQsLJQtKxC0ba2tmO2sNJSd5aXbfEH9uoL7AY2tACnUCcYnAAhGgHylebJRbpFagNYlTYeVS7XSuW6yVynBBPbAaaxIzT2VStJSODqvLqBXqobA-P6TYGMVxJLi8JYZVZmeTWJT5J5KFEWWTMHmyDlo8xWWz2BpNEqtZ6dAIjL6-fD-QCMmoBSWNBjLSy0ybOY2BuWMJrhajQqxkQriRmMu9QOgrsFg0xLeZOC2BIgExUwD30RRALOJgFXozUpJkQ3XZVwaWRbRFyZgcooeE4WhBlRSqRzqHkctp8iwu0kKimASW9AKH6gCnEwB8phRAAbKgApXQDHkUw2AtQzrWQgnNgitJpNZVPJmJH3ISRTVxfHmPJDvDp7IC-KPhSYoAG00AjDqAK78KIAY7QBwfB7ahiEemynvbug+xrQFaIxWKRKOYRx2BXcC-eIUAG1nkKioOjNhkQ21Fkj3RBRlB0WwrVcOo+zRY5FEOVwykJBxHA8d9Ai-H9wmieJ9zbEDJDMJRXH1aRVCTKcjg0J8DBTOFMQKCwcSOUiNkwghsMoQALCMABudAGylQAh5UAB1N8JbMFCMhYjslkFRlFUFQhWvK5+yUeD3Exep5EjNM+yfTj8G4ihwhIcTAIPIi1lI8jKMsaiWifVEU1I6NaIUXSyn0-MXldLjv1CT9AFNrMSEgkrVmWktZcnyQppEeNQLAFax4KKbsWJQgoM2sDDfMLIyAoGYYgn+WYCOAqKzD7VwbAOVppCnBo5JKNEilUfIMtcJL5F0FFDO4yklX+WlwqAyLw1MHqapYlwnDkQdaPNKpMxsFj3E0IddmnfrCqpZUaRBelWwqibHmsbAyLk5jB1NJxUsQzqnAcF9VB2xVqQmdVyvGjtJujQVrnsUj8TsDZWq7bYij5DY+WuN6Swrat6ybb6ww7LsezkUinCdDx7vS5CYN0K5KKUeHsC-H1-SDUbLMqiMoxjFEVFuHkVGTKpr3yJQeadeKozkaRDMARlVAFWbHCwjMsKLKkiadkUMjFLuWi1HQtFkKfTE5MjHkYLkHy5XeMWfxoBhUcPGT5ogyioKtOpZHVjxFAoxwcqjKdbFevLF2N0JIliaXjp+0DTCtJKLqHcoPHZod1JTEdNn7Rxla0QdZC9w3Al9ih+OE0LzassxZDubsHHsJ14SxbF1aRKwWg0eK+Q2huM5JH3xcC0TzKDtGQ5irK2pQs91bsGqWj0W4dmaZ1vaNjuKDFkLu8kk7fqFc7XEUnqBUJApp1UR34oNbK3f5dkKJF+evyXwOV+DmTzFafIcpcFQ6M0JF1cHKwo1Iu4UV7A0S+fQhgEDFqVOYtNZa-TsHkZg9QLDxXmrYXQ6t7A1WxHJBqRwKLwLJrPLOHdBofS+lA1eIdBwGnuEaNot03D0SqCaSh7hHA8hYm4No3gCFgKIXtYaIIyH32hJoLYZEG4bCxPCQ4KV45XEQhoO4HJNByQOMAikfCDp0hluQh+ZQrBHFoQ1BQfJFGH2difYuZ9PZqOwGLKmgYC701DozHqj4y7xnxHHRhA5FD1A8ByVok81A2LLFWWsjYAI9wtlkYuBpsSURojsJ4TozHH1dpYj2F9uH4EAA8agAzbRNn+M2gje6W3AioG2Tw7b2DgimHqbQDQewOLiY4+DM4EHyRLKWjiJoN02HOJ6bhGgwjRD1GolgDjIQaPGNmhlOl+zwrfCKpS1hh31GRZCAsY7uFGaRKwDUSg2zKAOBwcyCm8UEl3JZY0VlmCtNcZQm9Dj3CHP2W4ozLjnSSixKpahBRtLbu8eZJlPxXJ6b9Ro+oEw6CjJGLQMiqiPD5N2QkyICgeCjGcn8YswUlOiWYfuhRiilAYceFE5iyIUQFiaIUWLKCAEfbQAZXrL2Wfi6ohILqWDsIpPYbUPkNWUB7PERwigAr8rk85FB8k33BSHOS+okrwmoWzIcoz041QObcRBdxWl0pBTKvFhcIxDmmv2PMjgGq3HkGqhux8KKbwKNcDMerF750NU4reWxCj8lsEOO4HNjy9k2McNwckTE0TpUVDpeSIGyofnIc6agTTHCtNOWwpLUwbGjI4dkhwHDshJpGjRgIBHaKEWyEROkXB2E8FIoo-L9SlH7AtEGcki1DU0UdO+tzsgoUxH2dy6goxOEuGqhpmrmk6qOO2khGp3WnXItQycZreZLWPFm5+MJd6IKuELbJ8zsChKRhEuNMS8jVT5D1WwLE1Jjo1U07VvMxX5QPfk+xNMy09ucdGVxTDWbNQDamC12AHBkXLshXY-ZMJlkAHtq1YhhwdPWyRB+Ri5-0usXHWoyppcmOAODYBRezyGg6WODILcWfrZZNEu7g7jDksEOnq6tdiJ3ivAjwSIdat1dLB6sucKNRKNaHSljypEvKRIpUZhMQPIhrUaFmJGyOmWICym5VG+lbD5IMrqTQM2DngRdSpKITQ+L3ZnXjJl-aqbphNK0+JsDF3UJeO0OwpNOm7HC6ig58SXEU9WQAb3qAH346N8RABccgAHRAIAejNItId7TBEDm8ur4ijHcCiXjjxdQ1covszR2TzgIRZ6+brKNCY2aa+EDQLXxjcKMjxMYrTGnXngvzC9RYGtK043QnITkciuGBjYm9mPqFqPpjjV5CStdNpE7tVHYkHJ3v2Rbhx1aBMFfaW4tFLDbQIXBzclnunzt+hpgZC0dMjJTE8eEDmnDwjhYpFhmE9tbi-AJ2bQnIU2AQpRWFCj2S3mQooIUiUtuvzM4C7Az3fz-ji6YRonL4S5BQjZa1l3Ac3ZB9cV+jwnuVn27hAOsO1kR02dHacQ4HaXb7MGnGSJygZieLj-b-GSuCacfc4NTznyvOTreco50lVE0SklZ9AQoc4tZ+9pxfZ2rGnQ2iuQoyFEYKOCaa4QaCuZyh9KyXrKhMwlqPFK4LRKKVz0+IkDeGFHZRKLKCHUOmXWegSHWuBpIxXHgQKAcA4EXHmV6tdiaEnhRyZy94Kuu1NlZNd2M1VXfVWr5yI4HLFHAuCFAKUPbWOts4mp67zsNfU60Aw1DYjTVYl-ZChTCq4SA7j3Ed0CqllC3C6hlXlOUAc2gfEceoOVFLxWryuWvu4tE5-Rg4Va-IWgwS6tOVHnNFCfMBqzB44PXQ19GCP+QnXwwk0n11OMwed3q1uHEiTcg7j1CyZnVcX469xexOdahRN07xkjGiFaOwrTpx--zGeN+Vw78R8u09d6ZH9m96FdACgrxWohRNNv90ldAepB8gC9xt8x9G9rRn85BX8YCUwG5FBJkWEs1PlB9wER8G8ZIR0ICiZmhjh4oT8pxuwFEm1mIBYyDRZ78QDI8wD2QaCsYSh6D59LRbgoUWC8MBwFBuNSRVxyC0Cd8OwmprZDhNAzQyhP545eZlAWCnAFB7gko18ZCVw5DGBpAFDQIpCbtLA4RSJ1DfdUwyhoxd0tAWJDh05exB98l79KCshbAn8W85BZw+R1AG1BUC09Ymp6hPCY1gDzCqC+x+D04oZ3Ddkuoy9Lh3BaMmhoj790Cpdd8EjsCki5wQi6kChOR4Fwji5jdbByZVx9sKC4isgTwQNHgON8Q4VSIP8dgHMqkMxWgMwkIbF6itxGiMCZIWizx2i3AWCTRGD5FL8nZkI+xI0RjvCmjA1Tw2irsZi6MMtUxfUwiv98REw90XgRBuAIA4BxA3hxjVk9ESco47Byc5iUxJoRshcZl7gShLhNcIciw7iCUFFtCWZBxN4gZ9jQ54dEEPIG5sQWhQZDJyQo0ghATsgMiQTK8hxFId40E4FCgWh7hewRcuF2klxiF8A0TJppxYoeYnk2gFowZ8CFZbQhRU15opwkT3QvRvQqSdIqEtUORtYOQWh7C0xj5+w2M4VbhKIuS+gj00TsRuwTx2QFF2EBxnIqhTADhagPIihyh5piNslkSRi0TexlTewyJ4xdAzQD5LtyJXl+QSSvdpCAhABoL0AHi9QAIH1AAOPTRMon1H5wTEoh0gUEpy1Lpwcwp3skWhKANgh0AABUwAQes0TaIBcKJyh9hEEGMRRIz+R1okpYzjh4YqT9Dj4qJpxHIWI0RbsvVeRuVygZxyZQFUT8jfoMTCTydLhJD7CtB2p81NBa1IxdByY9o+SJ8HB-U3BSgephCEAoYHN2RFJLhVY-tyYFS2zMCLTcsVB4opl9jXIbB6gup04sQ4wjSySBpKZeTNzdENVqFGg2YkQeYsQ0QuZQZd5MxFJRc55yAqTNtMRN4-tVTL17DkIRsFEkD4oVU2gbEWyxZ-ylSOFgKtUFAwKeZ+knRN4pwYI1SLz-j8BfYKTxz+zPEsKZyFAT8Mx8gRww4UQlUfzCE+g7EbzQDTp7zBSnyRTXz44NTlA9AhR+RjERzskiKNy2LFD9l4kWIm09AR5SItgowwzLBRTp191zl-zgTkLU5UL-s6knhzpQM7M5Auoy5I0Wz8lEKaptLtgDg0LRlaEtgChrwiZbRXSgVzliLbzoQJyyLpypxZzUj9QsRpRrhdB9h3LAgD1xKeDwwGgz8VZvcdgp59jt4KU6LdZbhI031WLYrfp+TDQkUyI1B9I7TEUc0FJLpaJEx04-M+S0osjszY4dI10EBhyap2R7lZ9dgnVQ8+TYl009EhxHQfFbwBRCCpwMx4E4TpFB8SAzTYlwKPJ4R8Qpw7hO8MicoconQ9loYUDPwFq8hwKcpyhSchQM1HArBrgrQcx4w1pIrZDRZDqLo1ATq3Ao5zr5iHMWCbTHg1qHqVxLLvKZBFrXqFB3qlLbBQjBx2R7ApQeY6j1wNwzTNhMY2ZvlL1ujzoG47NlzlyTRhikaUbtyzy7rIKwL+sYxFjjykRSSCKD1TTgb0RyJsQihqI5pwKPkaSQqv9WamEzivAgA */
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