#pragma once

#include "../model/SearchResult.hpp"

namespace efcrawler {

// Replaces ResearchEngine::looksFree() / looksCommercial().
// Provider-supplied licence metadata wins over any heuristic.
class LicenseGate
{
public:
    [[nodiscard]] static Access judge(const SearchResult& result);

    [[nodiscard]] static bool shouldKeep(const SearchResult& result, bool freeOnly);

private:
    [[nodiscard]] static bool isFreeLicense(const QString& license);
};

} // namespace efcrawler
