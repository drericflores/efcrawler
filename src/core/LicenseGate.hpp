#pragma once

#include "../model/SearchResult.hpp"

namespace efcrawler {

// Centralizes the access decision formerly made inside ResearchEngine.
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
