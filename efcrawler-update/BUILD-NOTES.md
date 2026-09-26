# eFCrawler update package — how to apply it

Generated from the code review. **Nothing here has been compiled** — I have no
compiler. Treat the build as the real test.

## Contents

| Status | File |
|---|---|
| NEW | `src/model/ResourceType.hpp` |
| NEW | `src/model/ResourceClassifier.hpp` |
| NEW | `src/model/ResourceClassifier.cpp` |
| NEW | `src/model/MediaProbe.hpp` |
| NEW | `src/model/MediaProbe.cpp` |
| NEW | `src/core/LicenseGate.hpp` |
| NEW | `src/core/LicenseGate.cpp` |
| NEW | `src/providers/InternetArchiveProvider.hpp` |
| NEW | `src/providers/InternetArchiveProvider.cpp` |
| NEW | `src/providers/CommonsProvider.hpp` |
| NEW | `src/providers/CommonsProvider.cpp` |
| REPLACES | `src/model/SearchResult.hpp` |
| REPLACES | `src/core/ResearchEngine.hpp` |
| REPLACES | `src/core/ResearchEngine.cpp` |
| REPLACES | `src/providers/SearchProvider.hpp` |
| REPLACES | `src/providers/DuckDuckGoProvider.hpp` |
| REPLACES | `src/providers/DuckDuckGoProvider.cpp` |
| REPLACES | `src/providers/WikipediaProvider.hpp` |
| REPLACES | `src/providers/WikipediaProvider.cpp` |
| REPLACES | `src/providers/ProviderManager.hpp` |
| REPLACES | `src/providers/ProviderManager.cpp` |

## Apply

```bash
cd ~/path/to/efcrawler
git checkout -b media-update        # so `git checkout .` is a clean rollback
cp -r /path/to/efcrawler-update/src/. src/
```

## CMakeLists.txt — apply by hand

I have not seen your CMakeLists.txt, so these are the edits rather than a
replacement file. Confirm against your own.

1. Drop `Sql` from the Qt components and from `target_link_libraries` — no
   `QSql*` symbol exists in any source file. Keep `Core`, `Widgets`, `Network`.
2. Add the eleven new sources (`.cpp` files plus headers) to the target.
3. Add the version constants the new code references:

```cmake
target_compile_definitions(efcrawler PRIVATE
    EFCRAWLER_VERSION="${PROJECT_VERSION}"
)
```

Five files use `EFCRAWLER_VERSION`: `DuckDuckGoProvider.cpp`,
`WikipediaProvider.cpp`, `InternetArchiveProvider.cpp`, `CommonsProvider.cpp`,
`MediaProbe.cpp`. If the define is missing, those five fail to compile.

## main.cpp — two required edits

`SearchResult::type` and `::access` are now enums rather than `QString`, so the
result-row construction needs:

```cpp
// before
auto* typeItem   = new QStandardItem(result.type);
auto* accessItem = new QStandardItem(result.access);

// after
#include "model/ResourceType.hpp"
auto* typeItem   = new QStandardItem(efcrawler::resourceTypeLabel(result.type));
auto* accessItem = new QStandardItem(efcrawler::accessLabel(result.access));
```

Everything else compiles unchanged: every new `SearchResult` field is
default-initialised, and `ResearchEngine` still exposes `setFreeOnly()` and the
same signal set.

## NOT included

This package is the **data and provider layer**. It does not yet contain:

- The three checkboxes (Audio / Movies / Media) and their `QSettings` keys
- `ResultFilterProxy` and proxy-aware `selectedUrl()`
- Probe-on-selection wiring
- `DownloadManager` extraction and `safeFileName()` hardening
- `MainWindow` extraction from `main()`
- `.desktop` file, `QKeySequence`s, `tr()`
- `Qt Test` suites and CI

So the media buckets are not yet visible in the UI. `ResearchPlanOptions`
defaults to `freeOnly = true` and all three buckets off, which reproduces
today's six-query plan exactly. `setOptions()` is how you turn them on once the
UI exists.

## Known-unverified

1. **Archive.org `fl[]` encoding.** Bracket parameter names are percent-encoded
   in the request. If the response has no `docs` array, that is why. The
   endpoint is documented in the source comment.
2. **`-Wall -Wextra -Wpedantic`.** Not run. Expect a few unused-parameter notes
   in `LicenseGate` and possible `Q_UNUSED` needs.
3. **DuckDuckGo HTML selectors.** `result__a` and the `uddg` redirect parameter
   are current as of the review but are scraped, not contractual.
4. **`decodeHtml()` removed.** The replacement uses
   `QTextDocumentFragment::fromHtml()`. If the old function is still called
   anywhere in `main.cpp`, that call site now needs updating.

## Verify the three fixes

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/efcrawler
```

- **F1** — run the same topic twice without restarting. Before: second run
  showed an empty table. After: results both times.
- **F3** — search a topic with commercial PDFs. Before: they passed the
  free-only filter labelled "Free candidate". After: filtered out.
- **F2** — Start, Stop, Start immediately. Before: a plan entry was skipped and
  the progress counter over-reported. After: the full plan runs.

## Rollback

```bash
git checkout .          # or: git checkout main
```
