#pragma once

#include <filesystem>
#include <vector>

#include "codds/ModelLibrary.hpp"
#include "codds/ReverseCandidate.hpp"

namespace codds
{
    class ReverseObjectScanner
    {
    public:
        static std::vector<ReverseObjectInstance> scanObjects(
            const std::filesystem::path& path,
            const ReverseLayoutSettings& settings,
            const ModelLibrary* modelLibrary);
    };
}
