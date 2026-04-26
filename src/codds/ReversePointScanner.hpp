#pragma once

#include <filesystem>
#include <vector>

#include "codds/ReverseCandidate.hpp"

namespace codds
{
    class ReversePointScanner
    {
    public:
        static std::vector<ReversePoint> scanPoints(
            const std::filesystem::path& path,
            const ReverseLayoutSettings& settings
        );
    };
}
