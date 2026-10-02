// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "wave_model.h"
namespace nyako {
// Wire layout recovered from donor HeParse.java and parse_he_1_0/2_0_new.
// The renderer is a target-motor adaptation, not the proprietary AAC algorithm.
class HeModel {
public:
    explicit HeModel(std::string root):waves_(std::move(root)){}
    Wave render(const std::vector<int32_t>& packet,int loops,int interval,int amplitude,int frequency) const;
private: WaveModel waves_;
};
}
