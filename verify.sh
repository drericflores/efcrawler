#!/usr/bin/env bash
# =============================================================================
#  eFCrawler patch verifier
#
#  Confirms the update package is applied, that the binary reflects it, and
#  tells you which behavioural tests still need a human.
#
#  It cannot verify behaviour -- that needs the app running. It tells you
#  whether the other two levels are sound, so a behavioural result means
#  something.
#
#  Usage:
#     ./verify.sh                 # everything
#     ./verify.sh --quiet         # failures only
#     BIN=build/efcrawler ./verify.sh
#
#  Run from the repository root. Always exits 0; read the summary.
# =============================================================================

QUIET=0
[ "${1:-}" = "--quiet" ] && QUIET=1

BUILD_DIR="${BUILD_DIR:-build}"

c_bad=$'\033[31m'; c_ok=$'\033[32m'; c_warn=$'\033[33m'; c_dim=$'\033[2m'; c_off=$'\033[0m'

PASS=0; WARN=0; BAD=0

ok()   { PASS=$((PASS+1)); [ "$QUIET" -eq 0 ] && printf '  %sPASS%s %s\n' "$c_ok" "$c_off" "$*"; }
warn() { WARN=$((WARN+1));                     printf '  %sWARN%s %s\n' "$c_warn" "$c_off" "$*"; }
bad()  { BAD=$((BAD+1));                       printf '  %sFAIL%s %s\n' "$c_bad" "$c_off" "$*"; }
hdr()  { [ "$QUIET" -eq 0 ] && printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# --- preconditions -----------------------------------------------------------
if [ ! -d src ]; then
    printf '%sNot a repository root: no src/ here.%s\n' "$c_bad" "$c_off"
    exit 0
fi

# =============================================================================
#  LEVEL 1 -- did the files land?
# =============================================================================

hdr "Level 1: files"

NEW_FILES="
src/model/ResourceType.hpp
src/model/ResourceClassifier.hpp
src/model/ResourceClassifier.cpp
src/model/MediaProbe.hpp
src/model/MediaProbe.cpp
src/core/LicenseGate.hpp
src/core/LicenseGate.cpp
src/providers/InternetArchiveProvider.hpp
src/providers/InternetArchiveProvider.cpp
src/providers/CommonsProvider.hpp
src/providers/CommonsProvider.cpp
"

REPLACED_FILES="
src/model/SearchResult.hpp
src/core/ResearchEngine.hpp
src/core/ResearchEngine.cpp
src/providers/SearchProvider.hpp
src/providers/DuckDuckGoProvider.hpp
src/providers/DuckDuckGoProvider.cpp
src/providers/WikipediaProvider.hpp
src/providers/WikipediaProvider.cpp
src/providers/ProviderManager.hpp
src/providers/ProviderManager.cpp
"

MISSING=""
for f in $NEW_FILES $REPLACED_FILES; do
    [ -f "$f" ] || MISSING="$MISSING $f"
done

if [ -n "$MISSING" ]; then
    bad "missing files:$MISSING"
    printf '       apply the package, then re-run:  cp -r efcrawler-update/src/. src/\n'
else
    ok "all 21 files present"
fi

# Empty or truncated copies are the real risk, not absence.
SMALL=""
for f in $NEW_FILES $REPLACED_FILES; do
    [ -f "$f" ] || continue
    LINES=$(wc -l < "$f")
    # Smallest legitimately short file here is ResourceType.hpp (~150 lines).
    [ "$LINES" -lt 40 ] && SMALL="$SMALL $f(${LINES}l)"
done

if [ -n "$SMALL" ]; then
    bad "suspiciously short (truncated write?):$SMALL"
else
    ok "no truncated files"
fi

# =============================================================================
#  LEVEL 1b -- content markers
# =============================================================================

hdr "Level 1b: content markers"

has() {
    local label="$1" pat="$2"; shift 2
    local found=0
    for f in "$@"; do
        [ -f "$f" ] || continue
        grep -q -- "$pat" "$f" 2>/dev/null && found=1
    done
    if [ "$found" -eq 1 ]; then ok "$label"; else bad "$label  (pattern: $pat)"; fi
}

# F1 -- the DuckDuckGo fix
has "F1: seenUrls_ cleared per search" \
    'seenUrls_\.clear()' src/providers/DuckDuckGoProvider.cpp

# F2 -- generation counters
has "F2: generation counter on the base class" \
    'bumpGeneration' src/providers/SearchProvider.hpp
has "F2: providers bump it on cancel" \
    'bumpGeneration()' src/providers/DuckDuckGoProvider.cpp \
                       src/providers/WikipediaProvider.cpp
has "F2: manager resets currentProvider_ on cancel" \
    'currentProvider_ = -1' src/providers/ProviderManager.cpp

# F3 -- the licence gate replaced the substring heuristic
has "F3: LicenseGate exists" \
    'class LicenseGate' src/core/LicenseGate.hpp
has "F3: engine uses it" \
    'LicenseGate::judge' src/core/ResearchEngine.cpp

# F5 -- one classifier
has "F5: shared classifier" \
    'class ResourceClassifier' src/model/ResourceClassifier.hpp
has "F5: providers use it" \
    'ResourceClassifier::classify' src/providers/DuckDuckGoProvider.cpp \
                                   src/providers/WikipediaProvider.cpp

# Media layer
has "Media: bucket predicates" \
    'isMediaBucket' src/model/ResourceType.hpp
has "Media: probe" \
    'class MediaProbe' src/model/MediaProbe.hpp
has "Media: Archive.org provider" \
    'setMediatypeFilter' src/providers/InternetArchiveProvider.hpp
has "Media: Commons licence metadata" \
    'extmetadata' src/providers/CommonsProvider.cpp

# MediaInfo must be declared exactly once. If you pasted from the earlier
# chat artifacts instead of the package, it is declared in both files and
# the build fails with a redefinition error.
COUNT_SR=$(grep -c 'struct MediaInfo' src/model/SearchResult.hpp 2>/dev/null || true)
COUNT_MP=$(grep -c 'struct MediaInfo' src/model/MediaProbe.hpp 2>/dev/null || true)

if [ "${COUNT_MP:-0}" -eq 1 ] && [ "${COUNT_SR:-0}" -eq 0 ]; then
    ok "MediaInfo declared once, in MediaProbe.hpp"
elif [ "${COUNT_SR:-0}" -gt 0 ] && [ "${COUNT_MP:-0}" -gt 0 ]; then
    bad "MediaInfo declared in BOTH SearchResult.hpp and MediaProbe.hpp"
    printf '       You mixed an earlier chat artifact with the package.\n'
    printf '       Delete the copy in SearchResult.hpp.\n'
else
    warn "MediaInfo location unexpected (SearchResult=${COUNT_SR:-?} MediaProbe=${COUNT_MP:-?})"
fi

# =============================================================================
#  LEVEL 1c -- old code is gone
# =============================================================================

hdr "Level 1c: old code removed"

gone() {
    local label="$1" pat="$2"; shift 2
    local hits
    hits=$(grep -rn -- "$pat" "$@" 2>/dev/null | wc -l)
    if [ "$hits" -eq 0 ]; then ok "$label -- gone"; else
        bad "$label -- $hits reference(s) remain"
        grep -rn -- "$pat" "$@" 2>/dev/null | head -3 | sed 's/^/       /'
    fi
}

gone "F3: looksFree()"      'looksFree'      src/core
gone "F3: looksCommercial()" 'looksCommercial' src/core
gone "F5: classifyResource()" 'classifyResource' src/providers
gone "decodeHtml()"          'decodeHtml'     src/providers
gone "stripHtml()"           'stripHtml'      src/providers

# The "Free candidate" label string should be gone with the old engine.
if grep -rq 'Free candidate' src/ 2>/dev/null; then
    warn "'Free candidate' string still present -- check src/main.cpp"
else
    ok "old access labels gone"
fi

# The .pdf entry in the free-indicator list was the F3 bug.
PDF_HITS=$(grep -c '\.pdf' src/core/LicenseGate.cpp 2>/dev/null || true)
if [ "${PDF_HITS:-0}" -eq 0 ]; then
    ok "F3: no .pdf in the licence gate"
else
    bad "F3: .pdf appears in LicenseGate.cpp -- check it is not an indicator"
fi

# =============================================================================
#  LEVEL 1d -- CMakeLists.txt
# =============================================================================

hdr "Level 1d: CMakeLists.txt"

if [ ! -f CMakeLists.txt ]; then
    bad "no CMakeLists.txt here"
else
    CM="CMakeLists.txt"

    for src in \
        ResourceClassifier.cpp \
        LicenseGate.cpp \
        MediaProbe.cpp \
        InternetArchiveProvider.cpp \
        CommonsProvider.cpp
    do
        if grep -q "$src" "$CM"; then
            ok "listed: $src"
        else
            bad "NOT listed: $src  (will fail to link)"
        fi
    done

    if grep -q 'EFCRAWLER_VERSION' "$CM"; then
        ok "EFCRAWLER_VERSION defined"
    else
        bad "EFCRAWLER_VERSION not defined (5 files will not compile)"
        printf '       target_compile_definitions(efcrawler PRIVATE\n'
        printf '           EFCRAWLER_VERSION="${PROJECT_VERSION}")\n'
    fi

    if grep -qE 'Qt6::Sql|Qt6[^)]*\bSql\b' "$CM"; then
        warn "Qt6::Sql still referenced -- unused, but harmless"
    else
        ok "Qt6::Sql removed"
    fi
fi

# =============================================================================
#  LEVEL 2 -- is the binary current?
# =============================================================================

hdr "Level 2: binary"

BIN="${BIN:-}"
if [ -z "$BIN" ] && [ -d "$BUILD_DIR" ]; then
    BIN=$(find "$BUILD_DIR" -maxdepth 2 -type f -name efcrawler -perm -u+x 2>/dev/null | head -1)
fi

if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
    warn "no built binary found under ${BUILD_DIR}/"
    printf '       build first:  ./build.sh --clean\n'
else
    ok "binary: $BIN"

    # Header changes force a rebuild too, so check both extensions.
    NEWER=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' \) -newer "$BIN" 2>/dev/null | head -5)

    if [ -n "$NEWER" ]; then
        bad "binary is STALE -- these sources are newer:"
        printf '%s\n' "$NEWER" | sed 's/^/       /'
        printf '       rebuild:  ./build.sh --clean\n'
    else
        ok "binary newer than every source file"
    fi

    # The UA macro concatenates into one literal, so this proves the define
    # was seen by the compiler.
    if strings "$BIN" 2>/dev/null | grep -q 'eFCrawler/.*github.com/drericflores'; then
        ok "new User-Agent compiled in"
        if [ "$QUIET" -eq 0 ]; then
            strings "$BIN" 2>/dev/null \
                | grep 'eFCrawler/' | grep 'github.com/drericflores' \
                | head -1 | sed 's/^/       /'
        fi
    else
        bad "no eFCrawler/<version> (github.com/...) string in the binary"
        printf '       Either the define is missing or the build is stale.\n'
    fi

    # The old download UA from main.cpp. Expected to persist until the UI pass.
    if strings "$BIN" 2>/dev/null | grep -q 'eFCrawler/0\.2\.2'; then
        warn "old 'eFCrawler/0.2.2' string still present"
        printf '       Expected -- that User-Agent lives in main.cpp (F4),\n'
        printf '       which this package does not touch.\n'
    fi

    # Symbols survive unless stripped.
    if nm -C "$BIN" >/dev/null 2>&1; then
        if nm -C "$BIN" 2>/dev/null | grep -q 'ResourceClassifier::classify'; then
            ok "ResourceClassifier linked into the binary"
        else
            bad "ResourceClassifier::classify not in the binary -- stale build?"
        fi
        if nm -C "$BIN" 2>/dev/null | grep -q 'LicenseGate::judge'; then
            ok "LicenseGate linked into the binary"
        else
            bad "LicenseGate::judge not in the binary -- stale build?"
        fi
    else
        warn "binary stripped; skipping symbol checks"
    fi

    BUILT=$(stat -c '%y' "$BIN" 2>/dev/null | cut -d. -f1)
    [ -n "$BUILT" ] && [ "$QUIET" -eq 0 ] && printf '  %sbuilt: %s%s\n' "$c_dim" "$BUILT" "$c_off"
fi

# =============================================================================
#  LEVEL 3 -- behaviour, which this script cannot test
# =============================================================================

hdr "Level 3: behaviour -- manual"

cat <<'EOF'
  These need the app running. Source checks passing does not imply these do.

  [ ] F1  Research topic A. When complete, research topic A again
          WITHOUT restarting.
          FAIL: second run shows an empty table.
          PASS: results appear both times.

  [ ] F3  Search a topic with commercial PDFs, free-only ON.
          FAIL: paywalled PDFs appear, labelled "Free candidate".
          PASS: they are filtered out.

  [ ] F2  Start, Stop, Start again immediately.
          FAIL: a plan query is skipped, progress over-reports.
          PASS: the full plan runs; counter matches.

  [ ] Gen Start a plain topic, free-only ON, no media boxes.
          Plan should be 6 queries, same as before this package.
          PASS: it completes as it did in 0.2.3.
EOF

# =============================================================================
#  SUMMARY
# =============================================================================

printf '\n\033[1m==> Summary\033[0m\n'
printf '  %s%d passed%s   %s%d warnings%s   %s%d failed%s\n\n' \
    "$c_ok" "$PASS" "$c_off" \
    "$c_warn" "$WARN" "$c_off" \
    "$c_bad" "$BAD" "$c_off"

if [ "$BAD" -gt 0 ]; then
    printf '  %sLevels 1-2 have failures. Fix those before trusting any%s\n' "$c_bad" "$c_off"
    printf '  %sbehavioural result.%s\n\n' "$c_bad" "$c_off"
    exit 0
fi

printf '  %sLevels 1-2 clean.%s Now run the Level 3 checks -- source checks%s\n' \
    "$c_ok" "$c_off" "$c_off"
printf '  cannot prove behaviour.\n\n'

if [ -n "${BIN:-}" ] && [ -f "${BIN:-/nonexistent}" ]; then
    printf '  launch:  %s\n\n' "$BIN"
fi

exit 0