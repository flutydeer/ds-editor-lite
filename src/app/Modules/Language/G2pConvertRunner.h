#ifndef G2PCONVERTRUNNER_H
#define G2PCONVERTRUNNER_H

#include <vector>

#include <synthrt/G2P/LanguageService.h>

/// G2P 批量转换运行器。
///
namespace G2pConvertRunner {
    /// 批量执行原生 G2P 请求；调用方负责校验结果数量并提供回退。
    std::vector<srt::g2p::G2pRes> convert(const srt::g2p::LanguageService &service,
                                          const std::vector<srt::g2p::G2pInput> &requests);
}

#endif // G2PCONVERTRUNNER_H
