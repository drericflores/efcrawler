#!/usr/bin/env bash
# =============================================================================
#  eFCrawler 0.2.3 -> 0.3.0 update package
#
#  Materialises every file from the code review, then produces a tarball.
#
#  Usage:
#     ./build-package.sh                    # -> ./efcrawler-update.tar.gz
#     ./build-package.sh mydir myfile.tar.gz
#
#  Safe to re-run: it overwrites files it owns, touches nothing else.
#  Run it from the root of your efcrawler clone, or anywhere -- the output is
#  a self-contained directory you copy over the clone.
# =============================================================================
set -euo pipefail

OUT="${1:-efcrawler-update}"
TARBALL="${2:-efcrawler-update.tar.gz}"

echo "==> writing into ${OUT}/"
mkdir -p "${OUT}/src/core" "${OUT}/src/model" "${OUT}/src/providers"

# -----------------------------------------------------------------------------
#  MODEL LAYER
# -----------------------------------------------------------------------------

cat > "${OUT}/src/model/ResourceType.hpp" <<'EOFCRAWLER'
#pragma once

#include <QString>

namespace efcrawler {

// Coarse buckets. These reach the Type column and the QSettings keys for the
// media filter, so once shipped they are stable identifiers.
enum class ResourceType {
    Unknown = 0,
    Web,           // HTML landing pages, articles, wiki entries
    Pdf,
    Document,      // doc/docx/odt/rtf/txt/epub
    Spreadsheet,
    Presentation,
    Archive,       // zip/tar/gz/7z/iso
    Image,
    Audio,         // the "Audio" bucket
    Movie,         // the "Movies" bucket
    Media,         // the "Media" bucket: streams, playlists, feeds, item pages
    Data,          // json/xml/csv/geojson
    Executable
};

enum class Access {
    Unknown = 0,
    Free,          // positively identified as free / open / public domain
    Commercial
};

inline QString resourceTypeLabel(ResourceType type)
{
    switch (type) {
    case ResourceType::Unknown:      return QStringLiteral("Unknown");
    case ResourceType::Web:          return QStringLiteral("Web");
    case ResourceType::Pdf:          return QStringLiteral("PDF");
    case ResourceType::Document:     return QStringLiteral("Document");
    case ResourceType::Spreadsheet:  return QStringLiteral("Spreadsheet");
    case ResourceType::Presentation: return QStringLiteral("Presentation");
    case ResourceType::Archive:      return QStringLiteral("Archive");
    case ResourceType::Image:        return QStringLiteral("Image");
    case ResourceType::Audio:        return QStringLiteral("Audio");
    case ResourceType::Movie:        return QStringLiteral("Movie");
    case ResourceType::Media:        return QStringLiteral("Media");
    case ResourceType::Data:         return QStringLiteral("Data");
    case ResourceType::Executable:   return QStringLiteral("Executable");
    }
    return QStringLiteral("Unknown");
}

inline ResourceType resourceTypeFromLabel(const QString& label)
{
    static const struct { const char* label; ResourceType type; } kTable[] = {
        { "Web",          ResourceType::Web          },
        { "PDF",          ResourceType::Pdf          },
        { "Document",     ResourceType::Document     },
        { "Spreadsheet",  ResourceType::Spreadsheet  },
        { "Presentation", ResourceType::Presentation },
        { "Archive",      ResourceType::Archive      },
        { "Image",        ResourceType::Image        },
        { "Audio",        ResourceType::Audio        },
        { "Movie",        ResourceType::Movie        },
        { "Media",        ResourceType::Media        },
        { "Data",         ResourceType::Data         },
        { "Executable",   ResourceType::Executable   },
    };

    for (const auto& row : kTable) {
        if (label.compare(QLatin1String(row.label), Qt::CaseInsensitive) == 0) {
            return row.type;
        }
    }
    return ResourceType::Unknown;
}

inline QString accessLabel(Access access)
{
    switch (access) {
    case Access::Free:       return QStringLiteral("Free");
    case Access::Commercial: return QStringLiteral("Commercial");
    case Access::Unknown:    break;
    }
    return QStringLiteral("Unknown");
}

inline Access accessFromLabel(const QString& label)
{
    if (label.compare(QLatin1String("Free"), Qt::CaseInsensitive) == 0) {
        return Access::Free;
    }
    if (label.compare(QLatin1String("Commercial"), Qt::CaseInsensitive) == 0) {
        return Access::Commercial;
    }
    return Access::Unknown;
}

// --- Media buckets -----------------------------------------------------------
// The three user-facing checkboxes are disjoint, so they behave as filters
// rather than an overlapping hierarchy:
//     Audio  -> Audio
//     Movies -> Movie
//     Media  -> Image | Media   (images, playlists, streams, feeds, item pages)
//
// To make Media an umbrella over all three instead, OR the other two in.
inline bool isAudioBucket(ResourceType t) { return t == ResourceType::Audio; }
inline bool isMovieBucket(ResourceType t) { return t == ResourceType::Movie; }

inline bool isMediaBucket(ResourceType t)
{
    return t == ResourceType::Image || t == ResourceType::Media;
}

inline bool isMediaType(ResourceType t)
{
    return isAudioBucket(t) || isMovieBucket(t) || isMediaBucket(t);
}

inline bool isDownloadableType(ResourceType t)
{
    return t != ResourceType::Web && t != ResourceType::Unknown;
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/model/SearchResult.hpp" <<'EOFCRAWLER'
#pragma once

#include "ResourceType.hpp"

#include <QMetaType>
#include <QString>
#include <QUrl>

namespace efcrawler {

struct SearchResult
{
    QString title;
    ResourceType type{ResourceType::Unknown};
    QString source;                  // host, for the Source column
    QString size;                    // display string; filled by MediaProbe
    Access access{Access::Unknown};  // set centrally by LicenseGate
    QUrl url;

    // --- new ---
    QString provider;                // "DuckDuckGo", "Internet Archive", ...
    QString mime;                    // classifier guess, or authoritative MIME
    QString license;                 // provider-supplied, e.g. "CC BY-SA 4.0"
    QString licenseUrl;
    bool downloadable{false};
    bool streamable{false};
    bool ambiguous{false};           // extension alone is not decisive
};

} // namespace efcrawler

Q_DECLARE_METATYPE(efcrawler::SearchResult)
EOFCRAWLER

cat > "${OUT}/src/model/ResourceClassifier.hpp" <<'EOFCRAWLER'
#pragma once

#include "ResourceType.hpp"

#include <QString>
#include <QStringView>
#include <QUrl>

namespace efcrawler {

struct TypeInfo {
    ResourceType type{ResourceType::Unknown};
    QString mime;
    bool downloadable{false};
    bool streamable{false};
    bool ambiguous{false};
};

class ResourceClassifier
{
public:
    // URL-only. Never performs I/O, never blocks.
    [[nodiscard]] static TypeInfo classify(const QUrl& url);

    // Upgrade a guess with an authoritative Content-Type. Catches
    // "promised .mp4, served text/html".
    [[nodiscard]] static TypeInfo refine(const TypeInfo& guess,
                                        QStringView contentType);

    // Lowercased extension from the last dotted segment of the final path
    // component. Handles "/film.mp4/", "/song.MP3", percent-encoded paths.
    [[nodiscard]] static QString suffixOf(const QUrl& url);

    // Suffix to append when a download URL has no usable filename, e.g.
    // "https://host/download?id=7". Empty means "don't guess".
    [[nodiscard]] static QString canonicalSuffix(ResourceType type,
                                                 QStringView mime = {});

    // Human-readable size, or the em dash placeholder used today.
    [[nodiscard]] static QString formatSize(qint64 bytes);
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/model/ResourceClassifier.cpp" <<'EOFCRAWLER'
#include "ResourceClassifier.hpp"

#include <QHash>
#include <QLocale>
#include <QSet>

namespace efcrawler {
namespace {

using SuffixTable = QHash<QString, ResourceType>;

// This single table replaces the two divergent classifyResource() copies.
const SuffixTable& suffixTable()
{
    static const SuffixTable table = [] {
        SuffixTable t;

        const auto add = [&t](ResourceType type,
                              std::initializer_list<const char*> suffixes) {
            for (const char* suffix : suffixes) {
                t.insert(QString::fromLatin1(suffix), type);
            }
        };

        add(ResourceType::Pdf, { "pdf" });

        add(ResourceType::Document, {
            "doc", "docx", "odt", "rtf", "txt", "text", "md", "markdown",
            "rst", "tex", "epub", "mobi", "azw", "azw3", "fb2", "djvu",
            "ps", "pages", "wpd", "abw", "sxw", "fodt" });

        add(ResourceType::Spreadsheet, {
            "xls", "xlsx", "xlsm", "ods", "csv", "tsv", "numbers",
            "sxc", "fods", "gnumeric" });

        add(ResourceType::Presentation, {
            "ppt", "pptx", "pps", "ppsx", "odp", "key", "sxi", "fodp" });

        add(ResourceType::Archive, {
            "zip", "tar", "gz", "tgz", "bz2", "tbz", "tbz2", "xz", "txz",
            "lz", "lzma", "zst", "7z", "rar", "cab", "arj", "lha", "lzh",
            "cpio", "iso", "img", "dmg", "deb", "rpm", "apk",
            "jar", "war", "ear", "whl", "crx", "xpi" });

        add(ResourceType::Image, {
            "jpg", "jpeg", "jpe", "jfif", "png", "apng", "gif", "webp",
            "avif", "bmp", "dib", "tif", "tiff", "svg", "svgz", "ico",
            "heic", "heif", "jxl", "exr", "hdr", "tga", "psd", "xcf",
            "dng", "cr2", "cr3", "nef", "arw", "orf", "rw2", "raf",
            "pef", "srw", "raw", "pbm", "pgm", "ppm", "pnm", "xbm",
            "xpm", "qoi", "j2k", "jp2", "psb" });

        // Audio bucket
        add(ResourceType::Audio, {
            "mp3", "mp2", "mpa", "m4a", "m4b", "m4p",
            "aac", "ac3", "eac3", "dts", "flac", "alac",
            "ogg", "oga", "opus", "spx", "wav", "wave", "wma",
            "aif", "aiff", "aifc", "au", "snd", "caf", "voc", "gsm",
            "mid", "midi", "kar", "mka", "ape", "wv", "tta", "tak",
            "mpc", "amr", "awb", "dsf", "dff",
            "mod", "s3m", "xm", "it", "mtm", "umx" });

        // Movies bucket
        add(ResourceType::Movie, {
            "mp4", "m4v", "mkv", "webm", "avi", "mov", "qt",
            "wmv", "asf", "flv", "f4v", "swf", "mpg", "mpeg", "mpe",
            "m1v", "m2v", "mpv", "vob", "ogv", "ogm", "3gp", "3g2",
            "3gpp", "3gpp2", "ts", "m2ts", "mts", "m4s", "dv",
            "divx", "xvid", "rm", "rmvb", "mxf", "roq", "y4m",
            "dav", "vro", "wtv", "nut", "h264", "h265", "hevc", "av1" });

        // Media bucket: streams, containers, subtitle tracks
        add(ResourceType::Media, {
            "m3u", "m3u8", "pls", "xspf", "mpd", "asx", "wax", "wvx",
            "wpl", "cue", "smil", "smi", "ram", "qtl", "ism", "ismc",
            "webvtt", "vtt", "srt", "ass", "ssa", "sub", "idx", "sbv" });

        add(ResourceType::Data, {
            "json", "jsonl", "ndjson", "xml", "yaml", "yml", "toml",
            "ini", "cfg", "conf", "properties", "sql", "sqlite", "db",
            "parquet", "arrow", "feather", "h5", "hdf5", "nc", "grib",
            "geojson", "kml", "kmz", "shp", "gpx", "vcf", "ics",
            "rss", "atom", "pem", "crt", "cer", "der", "p12" });

        add(ResourceType::Executable, {
            "exe", "msi", "dll", "so", "dylib", "bin", "elf", "o", "obj",
            "appimage", "snap", "flatpak", "ref", "bat", "cmd", "com",
            "sh", "bash", "zsh", "fish", "ps1", "wasm", "run", "out" });

        add(ResourceType::Web, {
            "html", "htm", "xhtml", "shtml", "php", "asp", "aspx",
            "jsp", "cgi", "do", "action" });

        return t;
    }();

    return table;
}

// Extensions whose meaning depends on the container, not the name:
//   .ogg  = Vorbis audio or Theora video      .ts   = MPEG-TS or TypeScript
//   .mp4  = video, or audio-only              .webm = video, or Opus audio
const QSet<QString>& ambiguousSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("ogg"), QStringLiteral("ogv"), QStringLiteral("oga"),
        QStringLiteral("mp4"), QStringLiteral("m4a"), QStringLiteral("m4b"),
        QStringLiteral("webm"), QStringLiteral("mka"), QStringLiteral("mkv"),
        QStringLiteral("ts"),  QStringLiteral("m2ts"), QStringLiteral("mts"),
        QStringLiteral("ogm"), QStringLiteral("mpg"),  QStringLiteral("mpeg"),
    };
    return set;
}

struct HostHint {
    const char* host;
    ResourceType type;
    bool streamable;
};

// Ordered most-specific first; first match wins.
const HostHint kHostHints[] = {
    { "youtube.com",           ResourceType::Movie, true  },
    { "youtu.be",              ResourceType::Movie, true  },
    { "m.youtube.com",         ResourceType::Movie, true  },
    { "vimeo.com",             ResourceType::Movie, true  },
    { "dailymotion.com",       ResourceType::Movie, true  },
    { "twitch.tv",             ResourceType::Movie, true  },
    { "bitchute.com",          ResourceType::Movie, true  },
    { "odysee.com",            ResourceType::Movie, true  },
    { "rumble.com",            ResourceType::Movie, true  },
    { "ted.com",               ResourceType::Movie, true  },

    { "soundcloud.com",        ResourceType::Audio, true  },
    { "mixcloud.com",          ResourceType::Audio, true  },
    { "spotify.com",           ResourceType::Audio, true  },
    { "deezer.com",            ResourceType::Audio, true  },
    { "music.apple.com",       ResourceType::Audio, true  },
    { "bandcamp.com",          ResourceType::Audio, false },
    { "freesound.org",         ResourceType::Audio, false },
    { "freemusicarchive.org",  ResourceType::Audio, false },
    { "ccmixter.org",          ResourceType::Audio, false },

    // Podcast hosts: the page is HTML but the payload is audio.
    { "libsyn.com",            ResourceType::Audio, false },
    { "buzzsprout.com",        ResourceType::Audio, false },
    { "podbean.com",           ResourceType::Audio, false },
    { "simplecast.com",        ResourceType::Audio, false },
    { "megaphone.fm",          ResourceType::Audio, false },
    { "acast.com",             ResourceType::Audio, false },
    { "anchor.fm",             ResourceType::Audio, false },
    { "podcasts.apple.com",    ResourceType::Audio, true  },

    { "openverse.org",         ResourceType::Media, false },
    { "commons.wikimedia.org", ResourceType::Media, false },
    { "pixabay.com",           ResourceType::Media, false },
    { "pexels.com",            ResourceType::Media, false },
    { "unsplash.com",          ResourceType::Media, false },
    { "flickr.com",            ResourceType::Media, false },
    { "wikimedia.org",         ResourceType::Media, false },
    { "archive.org",           ResourceType::Media, false },
};

const HostHint* hostHintFor(const QString& host)
{
    if (host.isEmpty()) {
        return nullptr;
    }

    for (const HostHint& hint : kHostHints) {
        const QLatin1StringView needle(hint.host);
        if (host == needle ||
            host.endsWith(QStringLiteral(".") + QLatin1String(hint.host),
                          Qt::CaseInsensitive)) {
            return &hint;
        }
    }
    return nullptr;
}

QString mimeForSuffix(const QString& suffix)
{
    static const QHash<QString, QString> table = {
        { "pdf",  "application/pdf" },      { "epub", "application/epub+zip" },
        { "zip",  "application/zip" },      { "gz",   "application/gzip" },
        { "json", "application/json" },     { "xml",  "application/xml" },
        { "mp3",  "audio/mpeg" },           { "m4a",  "audio/mp4" },
        { "aac",  "audio/aac" },            { "flac", "audio/flac" },
        { "ogg",  "audio/ogg" },            { "oga",  "audio/ogg" },
        { "opus", "audio/opus" },           { "wav",  "audio/wav" },
        { "wma",  "audio/x-ms-wma" },       { "mid",  "audio/midi" },
        { "mp4",  "video/mp4" },            { "m4v",  "video/x-m4v" },
        { "mkv",  "video/x-matroska" },     { "webm", "video/webm" },
        { "avi",  "video/x-msvideo" },      { "mov",  "video/quicktime" },
        { "mpg",  "video/mpeg" },           { "mpeg", "video/mpeg" },
        { "ogv",  "video/ogg" },            { "wmv",  "video/x-ms-wmv" },
        { "flv",  "video/x-flv" },
        { "m3u8", "application/vnd.apple.mpegurl" },
        { "mpd",  "application/dash+xml" },
        { "jpg",  "image/jpeg" },           { "jpeg", "image/jpeg" },
        { "png",  "image/png" },            { "gif",  "image/gif" },
        { "webp", "image/webp" },           { "avif", "image/avif" },
        { "svg",  "image/svg+xml" },        { "tif",  "image/tiff" },
        { "tiff", "image/tiff" },           { "heic", "image/heic" },
    };

    return table.value(suffix);
}

QString mimeForType(ResourceType type)
{
    switch (type) {
    case ResourceType::Pdf:   return QStringLiteral("application/pdf");
    case ResourceType::Web:   return QStringLiteral("text/html");
    case ResourceType::Audio: return QStringLiteral("audio/mpeg");
    case ResourceType::Movie: return QStringLiteral("video/mp4");
    default:                  return {};
    }
}

ResourceType typeForQuery(const QUrl& url)
{
    if (!url.hasQuery()) {
        return ResourceType::Unknown;
    }

    const QString query = url.query().toLower();

    static const char* kAudioKeys[] = {
        "format=mp3", "format=flac", "format=ogg", "format=opus",
        "format=wav", "format=m4a", "ext=mp3", "type=audio",
    };
    static const char* kMovieKeys[] = {
        "format=mp4", "format=webm", "format=mkv", "format=ogv",
        "type=video", "type=movie",
    };

    for (const char* key : kAudioKeys) {
        if (query.contains(QLatin1String(key))) {
            return ResourceType::Audio;
        }
    }
    for (const char* key : kMovieKeys) {
        if (query.contains(QLatin1String(key))) {
            return ResourceType::Movie;
        }
    }
    return ResourceType::Unknown;
}

} // namespace

QString ResourceClassifier::suffixOf(const QUrl& url)
{
    const QString path = url.path();
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    const QString lastSegment = (slash >= 0) ? path.mid(slash + 1) : path;

    const qsizetype dot = lastSegment.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot == lastSegment.size() - 1) {
        return {};
    }

    const QString suffix = lastSegment.mid(dot + 1).trimmed().toLower();
    if (suffix.isEmpty() || suffix.size() > 12) {
        return {};
    }
    return suffix;
}

TypeInfo ResourceClassifier::classify(const QUrl& url)
{
    TypeInfo info;

    if (!url.isValid() || url.isEmpty()) {
        return info;
    }

    const QString suffix = suffixOf(url);
    const QString host = url.host().toLower();
    const QString path = url.path().toLower();

    // 1. A recognised extension is the strongest signal.
    if (!suffix.isEmpty()) {
        const ResourceType bySuffix =
            suffixTable().value(suffix, ResourceType::Unknown);

        if (bySuffix != ResourceType::Unknown) {
            info.type = bySuffix;
            info.mime = mimeForSuffix(suffix);
            info.ambiguous = ambiguousSuffixes().contains(suffix);
            info.downloadable = isDownloadableType(bySuffix) && !info.ambiguous;
        }
    }

    // 2. Extension said nothing: consult known hosts.
    if (info.type == ResourceType::Unknown) {
        if (const HostHint* hint = hostHintFor(host)) {
            info.type = hint->type;
            info.streamable = hint->streamable;
            info.downloadable = isDownloadableType(hint->type) && !hint->streamable;
        }
    }

    // 3. Player and embed endpoints.
    if (info.type == ResourceType::Unknown || info.type == ResourceType::Web) {
        if (path.contains(QLatin1String("/embed")) ||
            path.contains(QLatin1String("/player")) ||
            path.contains(QLatin1String("/watch")) ||
            path.contains(QLatin1String("/stream"))) {
            info.type = ResourceType::Media;
            info.streamable = true;
            info.downloadable = false;
        }
    }

    // 4. Query parameters: ?format=mp3, ?ext=flac, ?type=video
    if (const ResourceType byQuery = typeForQuery(url);
        byQuery != ResourceType::Unknown &&
        (info.type == ResourceType::Unknown || info.type == ResourceType::Web)) {
        info.type = byQuery;
        info.downloadable = true;
    }

    // 5. Default.
    if (info.type == ResourceType::Unknown) {
        info.type = ResourceType::Web;
    }

    if (info.mime.isEmpty()) {
        info.mime = mimeForType(info.type);
    }

    // A player page is never a plain download, whatever the extension claims.
    if (info.streamable || (isMediaType(info.type) && info.ambiguous)) {
        info.downloadable = false;
    }

    return info;
}

TypeInfo ResourceClassifier::refine(const TypeInfo& guess, QStringView contentType)
{
    TypeInfo info = guess;

    const QString mime =
        contentType.toString().section(QLatin1Char(';'), 0, 0).trimmed().toLower();

    if (mime.isEmpty()) {
        return info;   // server gave nothing; trust the URL guess
    }

    info.mime = mime;
    info.ambiguous = false;

    static const QHash<QString, ResourceType> byMime = {
        { "application/pdf",               ResourceType::Pdf },
        { "application/epub+zip",          ResourceType::Document },
        { "application/zip",               ResourceType::Archive },
        { "application/x-tar",             ResourceType::Archive },
        { "application/gzip",              ResourceType::Archive },
        { "application/x-7z-compressed",   ResourceType::Archive },
        { "application/json",              ResourceType::Data },
        { "application/xml",               ResourceType::Data },
        { "application/rss+xml",           ResourceType::Data },
        { "application/atom+xml",          ResourceType::Data },
        { "application/vnd.apple.mpegurl", ResourceType::Media },
        { "application/x-mpegurl",         ResourceType::Media },
        { "application/dash+xml",          ResourceType::Media },
        { "application/vnd.ms-sstr+xml",   ResourceType::Media },
        { "application/ogg",               ResourceType::Media },
        { "text/vtt",                      ResourceType::Media },
        { "application/x-subrip",          ResourceType::Media },
        { "application/vnd.ms-excel",      ResourceType::Spreadsheet },
        { "application/vnd.ms-powerpoint", ResourceType::Presentation },
        { "application/msword",            ResourceType::Document },
    };

    if (const auto it = byMime.constFind(mime); it != byMime.constEnd()) {
        info.type = it.value();
        info.streamable = (mime == QLatin1String("application/vnd.apple.mpegurl") ||
                           mime == QLatin1String("application/x-mpegurl") ||
                           mime == QLatin1String("application/dash+xml") ||
                           mime == QLatin1String("application/vnd.ms-sstr+xml"));
        info.downloadable = isDownloadableType(info.type) && !info.streamable;
        return info;
    }

    if (mime.startsWith(QLatin1String("audio/"))) {
        info.type = ResourceType::Audio;
        info.streamable = false;
        info.downloadable = true;
        return info;
    }
    if (mime.startsWith(QLatin1String("video/"))) {
        info.type = ResourceType::Movie;
        info.streamable = false;
        info.downloadable = true;
        return info;
    }
    if (mime.startsWith(QLatin1String("image/"))) {
        info.type = ResourceType::Image;
        info.streamable = false;
        info.downloadable = true;
        return info;
    }

    // text/html is the most valuable answer: the URL is a page, not the file
    // its extension promised. This is how a ".mp4" that is really a landing
    // page stops being offered as a download.
    if (mime == QLatin1String("text/html") ||
        mime == QLatin1String("application/xhtml+xml")) {
        info.type = guess.streamable ? ResourceType::Media : ResourceType::Web;
        info.downloadable = false;
        return info;
    }

    // application/octet-stream and friends: keep the URL-based guess.
    info.type = guess.type;
    info.downloadable = isDownloadableType(info.type) && !info.streamable;
    return info;
}

QString ResourceClassifier::canonicalSuffix(ResourceType type, QStringView mime)
{
    if (!mime.isEmpty()) {
        const QString key =
            mime.toString().section(QLatin1Char(';'), 0, 0).trimmed().toLower();

        static const QHash<QString, QString> table = {
            { "audio/mpeg", "mp3" },   { "audio/mp3", "mp3" },
            { "audio/mp4", "m4a" },    { "audio/aac", "aac" },
            { "audio/flac", "flac" },  { "audio/x-flac", "flac" },
            { "audio/ogg", "ogg" },    { "audio/opus", "opus" },
            { "audio/wav", "wav" },    { "audio/x-wav", "wav" },
            { "audio/webm", "weba" },  { "audio/midi", "mid" },
            { "video/mp4", "mp4" },    { "video/webm", "webm" },
            { "video/x-matroska", "mkv" }, { "video/quicktime", "mov" },
            { "video/mpeg", "mpg" },   { "video/x-msvideo", "avi" },
            { "video/ogg", "ogv" },    { "video/x-flv", "flv" },
            { "image/jpeg", "jpg" },   { "image/png", "png" },
            { "image/gif", "gif" },    { "image/webp", "webp" },
            { "image/avif", "avif" },  { "image/svg+xml", "svg" },
            { "image/tiff", "tiff" },  { "image/heic", "heic" },
            { "application/pdf", "pdf" }, { "application/zip", "zip" },
            { "application/epub+zip", "epub" },
        };

        if (const auto it = table.constFind(key); it != table.constEnd()) {
            return it.value();
        }
    }

    // Only return a suffix when confident; empty means "keep the server's name".
    switch (type) {
    case ResourceType::Pdf:          return QStringLiteral("pdf");
    case ResourceType::Spreadsheet:  return QStringLiteral("csv");
    case ResourceType::Presentation: return QStringLiteral("pptx");
    case ResourceType::Archive:      return QStringLiteral("zip");
    case ResourceType::Data:         return QStringLiteral("json");
    default:                         return {};
    }
}

QString ResourceClassifier::formatSize(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("\u2014");
    }
    return QLocale::system().formattedDataSize(
        bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/model/MediaProbe.hpp" <<'EOFCRAWLER'
#pragma once

#include "ResourceType.hpp"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QUrl>

class QNetworkReply;

namespace efcrawler {

struct MediaInfo
{
    bool probed{false};
    bool ok{false};
    QString mime;
    qint64 bytes{-1};
    QString suggestedFileName;   // from Content-Disposition, if present
    QUrl finalUrl;
    QString error;
};

class MediaProbe final : public QObject
{
    Q_OBJECT

public:
    explicit MediaProbe(QObject* parent = nullptr);

    // Idempotent: a repeat call for the same URL replays the cached answer.
    void probe(const QUrl& url);

    [[nodiscard]] MediaInfo cached(const QUrl& url) const;

    void cancelAll();

signals:
    void probed(const QUrl& url, const efcrawler::MediaInfo& info);

private:
    struct State {
        QUrl url;
        QString key;
        QNetworkReply* reply{nullptr};
        bool usedRange{false};
        bool headerReady{false};
        MediaInfo info;
    };

    void pump();
    void startHead(const QSharedPointer<State>& state);
    void startRangeGet(const QSharedPointer<State>& state);
    void onFinished(QNetworkReply* reply, const QSharedPointer<State>& state);
    void finish(const QSharedPointer<State>& state);

    [[nodiscard]] static QString canonicalKey(const QUrl& url);
    [[nodiscard]] static QString fileNameFromDisposition(const QByteArray& header);

    QNetworkAccessManager network_;
    QQueue<QUrl> queue_;
    QSet<QString> queued_;
    QHash<QString, QSharedPointer<State>> active_;
    QHash<QString, MediaInfo> cache_;
};

} // namespace efcrawler

Q_DECLARE_METATYPE(efcrawler::MediaInfo)
EOFCRAWLER

cat > "${OUT}/src/model/MediaProbe.cpp" <<'EOFCRAWLER'
#include "MediaProbe.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace efcrawler {
namespace {

constexpr int kTimeoutMs     = 8000;
constexpr int kMaxConcurrent = 3;

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

// Statuses that mean "no HEAD for you" rather than "no resource".
bool headIsRefused(int status)
{
    return status == 400 || status == 403 || status == 405 || status == 501;
}

} // namespace

MediaProbe::MediaProbe(QObject* parent)
    : QObject(parent)
{
    network_.setTransferTimeout(kTimeoutMs);
}

QString MediaProbe::canonicalKey(const QUrl& url)
{
    return url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
}

MediaInfo MediaProbe::cached(const QUrl& url) const
{
    return cache_.value(canonicalKey(url));
}

void MediaProbe::probe(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty()) {
        return;
    }

    const QString key = canonicalKey(url);

    if (const auto it = cache_.constFind(key); it != cache_.constEnd()) {
        emit probed(url, it.value());   // replay for the newly selected row
        return;
    }

    if (queued_.contains(key) || active_.contains(key)) {
        return;
    }

    queue_.enqueue(url);
    queued_.insert(key);
    pump();
}

void MediaProbe::pump()
{
    while (active_.size() < kMaxConcurrent && !queue_.isEmpty()) {
        const QUrl url = queue_.dequeue();
        const QString key = canonicalKey(url);
        queued_.remove(key);

        auto state = QSharedPointer<State>::create();
        state->url = url;
        state->key = key;
        active_.insert(key, state);

        startHead(state);
    }
}

void MediaProbe::startHead(const QSharedPointer<State>& state)
{
    QNetworkRequest request(state->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "*/*");

    state->reply = network_.head(request);

    connect(state->reply, &QNetworkReply::finished, this,
            [this, state]() {
                QNetworkReply* reply = state->reply;
                state->reply = nullptr;
                onFinished(reply, state);
                if (reply) {
                    reply->deleteLater();
                }
                pump();
            });
}

void MediaProbe::startRangeGet(const QSharedPointer<State>& state)
{
    // Some CDNs refuse HEAD. A one-byte ranged GET is the portable fallback;
    // we abort as soon as headers arrive, so no body is transferred even if
    // the server ignores the Range header.
    QNetworkRequest request(state->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Range", "bytes=0-0");

    state->reply = network_.get(request);

    connect(state->reply, &QNetworkReply::metaDataChanged, this, [state]() {
        if (state->headerReady || !state->reply) {
            return;
        }
        state->headerReady = true;
        state->reply->abort();     // headers are all we wanted
    });

    connect(state->reply, &QNetworkReply::finished, this,
            [this, state]() {
                QNetworkReply* reply = state->reply;
                state->reply = nullptr;
                onFinished(reply, state);
                if (reply) {
                    reply->deleteLater();
                }
                pump();
            });
}

void MediaProbe::onFinished(QNetworkReply* reply, const QSharedPointer<State>& state)
{
    if (!reply) {
        return;
    }

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (headIsRefused(status) && !state->usedRange) {
        state->usedRange = true;
        startRangeGet(state);
        return;
    }

    // Our own abort after headers is a success, not a failure.
    const bool abortedAfterHeaders =
        state->headerReady &&
        reply->error() == QNetworkReply::OperationCanceledError;

    if (reply->error() != QNetworkReply::NoError && !abortedAfterHeaders && status == 0) {
        state->info.probed = true;
        state->info.ok = false;
        state->info.error = reply->errorString();
        finish(state);
        return;
    }

    state->info.probed = true;
    state->info.ok = true;
    state->info.mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    state->info.finalUrl = reply->url();
    state->info.suggestedFileName =
        fileNameFromDisposition(reply->rawHeader("Content-Disposition"));

    qint64 bytes = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    // A ranged reply carries the *total* size after the slash: "bytes 0-0/4096".
    if (state->usedRange) {
        const QString contentRange =
            QString::fromLatin1(reply->rawHeader("Content-Range"));

        const qsizetype slash = contentRange.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            const qint64 total = contentRange.mid(slash + 1).trimmed().toLongLong();
            if (total > 0) {
                bytes = total;
            }
        }
    }

    state->info.bytes = bytes > 0 ? bytes : -1;
    finish(state);
}

void MediaProbe::finish(const QSharedPointer<State>& state)
{
    active_.remove(state->key);
    cache_.insert(state->key, state->info);
    emit probed(state->url, state->info);
}

void MediaProbe::cancelAll()
{
    queue_.clear();
    queued_.clear();

    const auto states = active_.values();
    active_.clear();

    for (const auto& state : states) {
        if (state->reply) {
            QNetworkReply* reply = state->reply;
            state->reply = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
}

QString MediaProbe::fileNameFromDisposition(const QByteArray& header)
{
    if (header.isEmpty()) {
        return {};
    }

    const QString value = QString::fromUtf8(header);

    // RFC 5987 form first: filename*=UTF-8''My%20Song.mp3
    static const QRegularExpression extended(
        QStringLiteral(R"(filename\*\s*=\s*[^']*'[^']*'([^;]+))"),
        QRegularExpression::CaseInsensitiveOption);

    if (const auto match = extended.match(value); match.hasMatch()) {
        return QUrl::fromPercentEncoding(match.captured(1).trimmed().toUtf8());
    }

    // Plain form: filename="My Song.mp3"
    static const QRegularExpression plain(
        QStringLiteral(R"(filename\s*=\s*"?([^";]+)"?)"),
        QRegularExpression::CaseInsensitiveOption);

    if (const auto match = plain.match(value); match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return {};
}

} // namespace efcrawler
EOFCRAWLER

# -----------------------------------------------------------------------------
#  CORE
# -----------------------------------------------------------------------------

cat > "${OUT}/src/core/LicenseGate.hpp" <<'EOFCRAWLER'
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
EOFCRAWLER

cat > "${OUT}/src/core/LicenseGate.cpp" <<'EOFCRAWLER'
#include "LicenseGate.hpp"

#include <QStringList>

#include <algorithm>

namespace efcrawler {
namespace {

// Strong signals: these genuinely imply free / open / public-domain access.
const QStringList kStrongFree = {
    QStringLiteral("gutenberg"),          QStringLiteral("archive.org/details"),
    QStringLiteral("openaccess"),         QStringLiteral("open-access"),
    QStringLiteral("publicdomain"),       QStringLiteral("public-domain"),
    QStringLiteral("creativecommons"),    QStringLiteral("commons.wikimedia.org"),
    QStringLiteral("openverse"),          QStringLiteral("doaj.org"),
    QStringLiteral("core.ac.uk"),         QStringLiteral("arxiv.org"),
    QStringLiteral("zenodo.org"),         QStringLiteral("hal.science"),
};

// Matched against the *hostname*, so "storage" no longer matches "store".
const QStringList kCommercialHosts = {
    QStringLiteral("amazon."),       QStringLiteral("ebay."),
    QStringLiteral("walmart."),      QStringLiteral("barnesandnoble."),
    QStringLiteral("abebooks."),     QStringLiteral("audible."),
    QStringLiteral("udemy."),        QStringLiteral("coursera."),
    QStringLiteral("shutterstock."), QStringLiteral("envato."),
    QStringLiteral("istockphoto."),  QStringLiteral("gettyimages."),
};

// Matched against whole path segments, so "buyer" no longer matches "buy".
const QStringList kCommercialTokens = {
    QStringLiteral("checkout"),     QStringLiteral("cart"),
    QStringLiteral("pricing"),      QStringLiteral("subscribe"),
    QStringLiteral("subscription"), QStringLiteral("purchase"),
};

// Recognised open licences, for provider-supplied metadata.
const QStringList kFreeLicenceNames = {
    QStringLiteral("cc0"),          QStringLiteral("public domain"),
    QStringLiteral("cc by"),        QStringLiteral("cc-by"),
    QStringLiteral("creative commons"),
    QStringLiteral("gfdl"),         QStringLiteral("fdl"),
};

} // namespace

bool LicenseGate::isFreeLicense(const QString& license)
{
    const QString lower = license.toLower();

    return std::any_of(kFreeLicenceNames.cbegin(), kFreeLicenceNames.cend(),
                       [&lower](const QString& needle) {
                           return lower.contains(needle);
                       });
}

Access LicenseGate::judge(const SearchResult& result)
{
    // 1. Authoritative: the provider told us the licence.
    if (!result.license.isEmpty()) {
        return isFreeLicense(result.license) ? Access::Free : Access::Commercial;
    }

    const QString host = result.url.host().toLower();
    const QString path = result.url.path().toLower();

    const bool commercialHost =
        std::any_of(kCommercialHosts.cbegin(), kCommercialHosts.cend(),
                    [&host](const QString& needle) {
                        return host.contains(needle);
                    });

    const bool commercialPath =
        std::any_of(kCommercialTokens.cbegin(), kCommercialTokens.cend(),
                    [&path](const QString& token) {
                        return path.split(QLatin1Char('/')).contains(token);
                    });

    // No ".pdf" escape hatch: an extension is not a licence, so a paywalled
    // PDF on a commercial host is now correctly classified Commercial.
    if (commercialHost || commercialPath) {
        return Access::Commercial;
    }

    const QString haystack = host + QLatin1Char(' ') + path;

    const bool strongFree =
        std::any_of(kStrongFree.cbegin(), kStrongFree.cend(),
                    [&haystack](const QString& needle) {
                        return haystack.contains(needle);
                    });

    return strongFree ? Access::Free : Access::Unknown;
}

bool LicenseGate::shouldKeep(const SearchResult& result, bool freeOnly)
{
    if (!freeOnly) {
        return true;
    }
    // Unknown is kept: "we could not tell" is not the same as "paywalled",
    // and the UI labels the three states distinctly.
    return result.access != Access::Commercial;
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/core/ResearchEngine.hpp" <<'EOFCRAWLER'
#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

namespace efcrawler {

class SearchProvider;

struct ResearchPlanOptions
{
    bool freeOnly{true};
    bool wantAudio{false};
    bool wantMovies{false};
    bool wantMedia{false};
};

class ResearchEngine final : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Searching,
        Paused
    };
    Q_ENUM(State)

    explicit ResearchEngine(QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] QString topic() const;
    [[nodiscard]] int resultCount() const noexcept;
    [[nodiscard]] int queriesCompleted() const noexcept;
    [[nodiscard]] int totalQueries() const noexcept;
    [[nodiscard]] bool freeOnly() const noexcept;
    [[nodiscard]] ResearchPlanOptions options() const noexcept;

    void setFreeOnly(bool enabled);
    void setOptions(const ResearchPlanOptions& options);

public slots:
    void startResearch(const QString& topic);
    void pauseResearch();
    void resumeResearch();
    void stopResearch();

signals:
    void researchStarted(const QString& topic);
    void researchPaused();
    void researchResumed();
    void researchStopped();

    void resultDiscovered(const efcrawler::SearchResult& result);
    void resultsCleared();

    void statusChanged(const QString& status);

    void progressChanged(int queriesCompleted,
                         int totalQueries,
                         int resultsFound);

    void providerStatusChanged(const QString& provider,
                               const QString& status);

private:
    struct PlannedQuery {
        QString text;
        bool media{false};   // routed to the media chain, not the general one
    };

    void buildResearchPlan();
    void dispatchNextQuery();
    void finishResearch();

    void wireProvider(SearchProvider* provider, bool media);
    void handleResult(SearchResult result);

    QString topic_;
    State state_{State::Idle};

    QQueue<PlannedQuery> pendingQueries_;
    QSet<QString> seenUrls_;

    SearchProvider* general_{nullptr};   // DuckDuckGo -> Wikipedia
    SearchProvider* media_{nullptr};     // Internet Archive -> Wikimedia Commons
    SearchProvider* archive_{nullptr};   // kept so the mediatype filter can be set

    int queriesCompleted_{0};
    int totalQueries_{0};
    int resultCount_{0};

    bool generalAvailable_{true};
    bool mediaAvailable_{true};
    bool stopRequested_{false};
    bool pendingDispatch_{false};
    bool firstQuery_{true};

    // Jittered gap between consecutive queries against the scraped general
    // provider. Ten-plus back-to-back requests are what trip its challenge.
    int rateGapMs_{1400};

    ResearchPlanOptions options_;
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/core/ResearchEngine.cpp" <<'EOFCRAWLER'
#include "ResearchEngine.hpp"

#include "LicenseGate.hpp"
#include "../model/ResourceClassifier.hpp"
#include "../providers/CommonsProvider.hpp"
#include "../providers/DuckDuckGoProvider.hpp"
#include "../providers/InternetArchiveProvider.hpp"
#include "../providers/ProviderManager.hpp"
#include "../providers/SearchProvider.hpp"
#include "../providers/WikipediaProvider.hpp"

#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>

#include <utility>

namespace efcrawler {

ResearchEngine::ResearchEngine(QObject* parent)
    : QObject(parent)
{
    // --- general chain: scraped, so it gets the polite rate limit ---
    auto* general = new ProviderManager(this);
    general->addProvider(new DuckDuckGoProvider(general));
    general->addProvider(new WikipediaProvider(general));
    general_ = general;

    // --- media chain: documented JSON APIs, no scraping ---
    auto* media = new ProviderManager(this);

    archive_ = new InternetArchiveProvider(media);
    media->addProvider(archive_);
    media->addProvider(new CommonsProvider(media));
    media_ = media;

    wireProvider(general_, false);
    wireProvider(media_, true);
}

void ResearchEngine::wireProvider(SearchProvider* provider, bool media)
{
    connect(provider, &SearchProvider::resultFound,
            this, [this](SearchResult result) { handleResult(std::move(result)); });

    connect(provider, &SearchProvider::providerUnavailable, this,
            [this, media](const QString& name, const QString& reason) {
                if (media) {
                    mediaAvailable_ = false;
                } else {
                    generalAvailable_ = false;
                }

                emit providerStatusChanged(name, QStringLiteral("Unavailable"));
                emit statusChanged(QStringLiteral("%1 unavailable — %2")
                                       .arg(name, reason));
            });

    connect(provider, &SearchProvider::providerError, this,
            [this](const QString& name, const QString& message) {
                emit providerStatusChanged(name, QStringLiteral("Error"));
                emit statusChanged(QStringLiteral("%1 error — %2")
                                       .arg(name, message));
            });

    connect(provider, &SearchProvider::searchFinished, this, [this]() {
        if (stopRequested_) {
            return;
        }

        ++queriesCompleted_;

        emit progressChanged(queriesCompleted_, totalQueries_, resultCount_);

        if (state_ == State::Searching) {
            dispatchNextQuery();
        }
    });
}

void ResearchEngine::handleResult(SearchResult result)
{
    const QString canonical =
        result.url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

    if (seenUrls_.contains(canonical)) {
        return;
    }
    seenUrls_.insert(canonical);

    // Providers that carry authoritative metadata (Archive.org mediatype,
    // Commons MIME) set `type` themselves; everything else is classified here
    // so both providers agree on what a given URL is.
    if (result.type == ResourceType::Unknown) {
        const TypeInfo info = result.mime.isEmpty()
            ? ResourceClassifier::classify(result.url)
            : ResourceClassifier::refine(ResourceClassifier::classify(result.url),
                                         result.mime);

        result.type = info.type;
        result.downloadable = info.downloadable;
        result.streamable = info.streamable;
        result.ambiguous = info.ambiguous;

        if (result.mime.isEmpty()) {
            result.mime = info.mime;
        }
    }

    if (result.size.isEmpty()) {
        result.size = ResourceClassifier::formatSize(-1);
    }

    // Single decision point, replacing looksFree()/looksCommercial().
    result.access = LicenseGate::judge(result);

    if (!LicenseGate::shouldKeep(result, options_.freeOnly)) {
        return;
    }

    ++resultCount_;

    emit resultDiscovered(result);
    emit progressChanged(queriesCompleted_, totalQueries_, resultCount_);
}

ResearchEngine::State ResearchEngine::state() const noexcept
{
    return state_;
}

QString ResearchEngine::topic() const
{
    return topic_;
}

int ResearchEngine::resultCount() const noexcept
{
    return resultCount_;
}

int ResearchEngine::queriesCompleted() const noexcept
{
    return queriesCompleted_;
}

int ResearchEngine::totalQueries() const noexcept
{
    return totalQueries_;
}

bool ResearchEngine::freeOnly() const noexcept
{
    return options_.freeOnly;
}

ResearchPlanOptions ResearchEngine::options() const noexcept
{
    return options_;
}

void ResearchEngine::setFreeOnly(bool enabled)
{
    options_.freeOnly = enabled;
}

void ResearchEngine::setOptions(const ResearchPlanOptions& options)
{
    options_ = options;
}

void ResearchEngine::startResearch(const QString& topic)
{
    const QString normalized = topic.trimmed();

    if (normalized.isEmpty()) {
        emit statusChanged(QStringLiteral("Enter a research topic."));
        return;
    }

    if (state_ != State::Idle) {
        stopResearch();
    }

    topic_ = normalized;

    pendingQueries_.clear();
    seenUrls_.clear();

    queriesCompleted_ = 0;
    resultCount_ = 0;

    generalAvailable_ = true;
    mediaAvailable_ = true;
    stopRequested_ = false;
    pendingDispatch_ = false;
    firstQuery_ = true;

    // Keep the media chain's mediatype filter in step with the checkboxes.
    if (auto* archive = dynamic_cast<InternetArchiveProvider*>(archive_)) {
        QStringList types;
        if (options_.wantAudio)  { types << QStringLiteral("audio"); }
        if (options_.wantMovies) { types << QStringLiteral("movies"); }
        if (options_.wantMedia)  { types << QStringLiteral("image") << QStringLiteral("texts"); }
        archive->setMediatypeFilter(types.join(QStringLiteral(" OR ")));
    }

    buildResearchPlan();

    state_ = State::Searching;

    emit resultsCleared();
    emit researchStarted(topic_);

    emit providerStatusChanged(general_->name(), QStringLiteral("Ready"));
    emit progressChanged(0, totalQueries_, 0);

    emit statusChanged(options_.freeOnly
        ? QStringLiteral("Free-resource research started: %1").arg(topic_)
        : QStringLiteral("Research started: %1").arg(topic_));

    dispatchNextQuery();
}

void ResearchEngine::pauseResearch()
{
    if (state_ != State::Searching) {
        return;
    }

    state_ = State::Paused;

    emit researchPaused();

    emit statusChanged(QStringLiteral(
        "Research paused. Current request may finish; no new request will start."));
}

void ResearchEngine::resumeResearch()
{
    if (state_ != State::Paused) {
        return;
    }

    state_ = State::Searching;

    emit researchResumed();
    emit statusChanged(QStringLiteral("Research resumed."));

    if (!general_->isBusy() && !media_->isBusy()) {
        dispatchNextQuery();
    }
}

void ResearchEngine::stopResearch()
{
    if (state_ == State::Idle) {
        return;
    }

    stopRequested_ = true;
    state_ = State::Idle;

    pendingQueries_.clear();

    general_->cancel();
    media_->cancel();

    emit statusChanged(QStringLiteral("Research stopped — %1 result(s) retained.")
                           .arg(resultCount_));

    emit researchStopped();
}

void ResearchEngine::buildResearchPlan()
{
    const QString t = topic_;
    const bool free = options_.freeOnly;

    const auto add = [this](const QString& text, bool media = false) {
        pendingQueries_.enqueue(PlannedQuery{ text, media });
    };

    // --- general ---
    if (free) {
        add(t + QStringLiteral(" free download"));
        add(t + QStringLiteral(" free PDF"));
        add(t + QStringLiteral(" open access"));
        add(t + QStringLiteral(" public domain"));
        add(QStringLiteral("\"%1\" free filetype:pdf").arg(t));
        add(t + QStringLiteral(" free manual OR documentation"));
    } else {
        add(t);
        add(t + QStringLiteral(" PDF"));
        add(t + QStringLiteral(" book"));
        add(t + QStringLiteral(" manual"));
        add(QStringLiteral("\"%1\" filetype:pdf").arg(t));
        add(t + QStringLiteral(" tutorial"));
    }

    // --- Audio ---
    if (options_.wantAudio) {
        if (free) {
            add(t + QStringLiteral(" filetype:mp3"));
            add(t + QStringLiteral(" public domain audio"));
            add(t + QStringLiteral(" podcast mp3 open license"));
            add(QStringLiteral("site:openverse.org \"%1\" audio").arg(t));
            add(QStringLiteral("\"%1\" creative commons music").arg(t), true);
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" audio").arg(t), true);
            add(QStringLiteral("site:archive.org \"%1\" mediatype:audio").arg(t), true);
        } else {
            add(t + QStringLiteral(" mp3"));
            add(t + QStringLiteral(" audio download"));
            add(t + QStringLiteral(" song OR album OR soundtrack"));
            add(QStringLiteral("site:archive.org \"%1\" mediatype:audio").arg(t), true);
        }
    }

    // --- Movies ---
    if (options_.wantMovies) {
        if (free) {
            add(t + QStringLiteral(" filetype:mp4"));
            add(t + QStringLiteral(" public domain film"));
            add(t + QStringLiteral(" creative commons video"));
            add(t + QStringLiteral(" open movie download"));
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" video").arg(t), true);
            add(QStringLiteral("site:archive.org \"%1\" mediatype:movies").arg(t), true);
        } else {
            add(t + QStringLiteral(" mp4"));
            add(t + QStringLiteral(" video download"));
            add(t + QStringLiteral(" trailer OR documentary OR film"));
            add(QStringLiteral("site:archive.org \"%1\" mediatype:movies").arg(t), true);
        }
    }

    // --- Media (images, galleries, item pages) ---
    if (options_.wantMedia) {
        if (free) {
            add(t + QStringLiteral(" public domain images"));
            add(t + QStringLiteral(" creative commons images"));
            add(QStringLiteral("site:openverse.org \"%1\"").arg(t), true);
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" filetype:bitmap").arg(t), true);
        } else {
            add(t + QStringLiteral(" images"));
            add(t + QStringLiteral(" screenshots OR gallery"));
        }
    }

    totalQueries_ = pendingQueries_.size();
}

void ResearchEngine::dispatchNextQuery()
{
    if (state_ != State::Searching || pendingDispatch_) {
        return;
    }

    // Skip plan entries whose chain is unavailable, and wait if a chain is
    // still busy with the previous query.
    while (!pendingQueries_.isEmpty()) {
        const PlannedQuery& next = pendingQueries_.head();

        if (next.media && !mediaAvailable_) {
            pendingQueries_.dequeue();
            continue;
        }
        if (!next.media && !generalAvailable_) {
            pendingQueries_.dequeue();
            continue;
        }

        SearchProvider* provider = next.media ? media_ : general_;

        if (provider->isBusy()) {
            return;
        }
        break;
    }

    if (pendingQueries_.isEmpty()) {
        finishResearch();
        return;
    }

    const PlannedQuery next = pendingQueries_.dequeue();

    SearchProvider* provider = next.media ? media_ : general_;
    const QString query = next.text;

    emit providerStatusChanged(provider->name(), QStringLiteral("Searching"));
    emit statusChanged(QStringLiteral("%1 — searching: %2")
                           .arg(provider->name(), query));

    // The scraped endpoint gets a jittered gap; the JSON APIs do not need one.
    // The first query is immediate so Start still feels responsive.
    const int delayMs = firstQuery_
        ? 0
        : (next.media ? 150
                      : rateGapMs_ + QRandomGenerator::global()->bounded(0, 500));

    firstQuery_ = false;

    if (delayMs == 0) {
        provider->search(query);
        return;
    }

    pendingDispatch_ = true;

    QTimer::singleShot(delayMs, this, [this, provider, query]() {
        pendingDispatch_ = false;

        if (state_ != State::Searching || stopRequested_) {
            return;
        }

        provider->search(query);
    });
}

void ResearchEngine::finishResearch()
{
    if (state_ == State::Idle) {
        return;
    }

    state_ = State::Idle;

    if (!generalAvailable_ && resultCount_ == 0) {
        emit statusChanged(QStringLiteral("Research provider unavailable. "
                                          "%1 result(s) retained.")
                               .arg(resultCount_));
    } else {
        emit providerStatusChanged(general_->name(), QStringLiteral("Ready"));

        emit statusChanged(QStringLiteral("Research complete — %1 result(s) found.")
                               .arg(resultCount_));
    }

    emit researchStopped();
}

} // namespace efcrawler
EOFCRAWLER

# -----------------------------------------------------------------------------
#  PROVIDERS
# -----------------------------------------------------------------------------

cat > "${OUT}/src/providers/SearchProvider.hpp" <<'EOFCRAWLER'
#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QString>

namespace efcrawler {

// Adds a generation counter. cancel() bumps it; reply handlers capture the
// value at request time and ignore any completion that arrives after it has
// moved on. That is what stops a cancelled request from being mistaken for
// the next one finishing.
class SearchProvider : public QObject
{
    Q_OBJECT

public:
    explicit SearchProvider(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~SearchProvider() override = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool isBusy() const noexcept = 0;

    [[nodiscard]] quint64 generation() const noexcept { return generation_; }

public slots:
    virtual void search(const QString& query) = 0;
    virtual void cancel() = 0;

signals:
    void resultFound(const efcrawler::SearchResult& result);
    void searchFinished();
    void providerUnavailable(const QString& provider, const QString& reason);
    void providerError(const QString& provider, const QString& message);

protected:
    void bumpGeneration() { ++generation_; }

private:
    quint64 generation_{0};
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/DuckDuckGoProvider.hpp" <<'EOFCRAWLER'
#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>
#include <QUrl>

namespace efcrawler {

class DuckDuckGoProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit DuckDuckGoProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    void releaseActiveReply();

    // Replaces a hand-rolled entity table plus a tag-stripping regex.
    [[nodiscard]] static QString plainTextFromHtml(QStringView html);
    [[nodiscard]] static QUrl extractTarget(const QUrl& url);
    [[nodiscard]] static bool looksLikeChallenge(const QString& html);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/DuckDuckGoProvider.cpp" <<'EOFCRAWLER'
#include "DuckDuckGoProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>

namespace efcrawler {
namespace {

constexpr auto kBrowserUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/150.0 Safari/537.36";

} // namespace

DuckDuckGoProvider::DuckDuckGoProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString DuckDuckGoProvider::name() const
{
    return QStringLiteral("DuckDuckGo");
}

bool DuckDuckGoProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void DuckDuckGoProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    // Without this the set accumulated for the whole process lifetime, so every
    // URL surfaced by an earlier query -- or by an earlier research run in the
    // same session -- was silently dropped from later ones, and re-running the
    // same topic produced an empty table until restart. Cross-query dedup is
    // the engine's job (ResearchEngine::seenUrls_).
    seenUrls_.clear();

    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("q"), query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", kBrowserUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");

    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    const quint64 gen = generation();

    activeReply_ = network_.post(request, body);

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        // Cancelled since the request went out: the manager is not waiting for
        // this one any more, so emitting searchFinished() here would be read as
        // the *next* query completing.
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            return;
        }

        processReply(reply);
        reply->deleteLater();
    });
}

void DuckDuckGoProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;

    // Released *before* abort(), so a search() arriving on the same event-loop
    // turn is not refused by the `if (activeReply_) return;` guard above.
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void DuckDuckGoProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void DuckDuckGoProvider::processReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit searchFinished();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        emit searchFinished();
        return;
    }

    const QString html = QString::fromUtf8(reply->readAll());

    if (looksLikeChallenge(html)) {
        emit providerUnavailable(
            name(),
            QStringLiteral("The provider returned an automated-search "
                           "challenge instead of search results."));
        emit searchFinished();
        return;
    }

    static const QRegularExpression linkPattern(
        QStringLiteral(R"(<a[^>]*class=["'][^"']*result__a[^"']*["'][^>]*href=["']([^"']+)["'][^>]*>(.*?))"),
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator matches = linkPattern.globalMatch(html);

    while (matches.hasNext()) {
        const auto match = matches.next();

        const QUrl url =
            extractTarget(QUrl::fromEncoded(match.captured(1).toUtf8()));

        if (!url.isValid()) {
            continue;
        }

        const QString scheme = url.scheme().toLower();
        if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
            continue;
        }

        const QString canonical =
            url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = plainTextFromHtml(match.captured(2));

        if (result.title.isEmpty()) {
            result.title = url.fileName();
        }
        if (result.title.isEmpty()) {
            result.title = url.host();
        }

        // One shared classifier instead of this provider's own copy.
        const TypeInfo info = ResourceClassifier::classify(url);

        result.type = info.type;
        result.mime = info.mime;
        result.downloadable = info.downloadable;
        result.streamable = info.streamable;
        result.ambiguous = info.ambiguous;
        result.provider = name();
        result.source = url.host();
        result.size = ResourceClassifier::formatSize(-1);
        result.url = url;

        // access is deliberately left Unknown -- LicenseGate decides in the
        // engine, so both providers agree on what "free" means.

        emit resultFound(result);
    }

    emit searchFinished();
}

bool DuckDuckGoProvider::looksLikeChallenge(const QString& html)
{
    const QString lower = html.toLower();

    return lower.contains(QStringLiteral("challenge")) &&
           (lower.contains(QStringLiteral("anomaly")) ||
            lower.contains(QStringLiteral("automated")));
}

QString DuckDuckGoProvider::plainTextFromHtml(QStringView html)
{
    // QTextDocumentFragment understands the full HTML entity set, including
    // the numeric forms the previous hand-rolled table could not cover, and
    // strips tags in the same pass. simplified() collapses the whitespace.
    return QTextDocumentFragment::fromHtml(html.toString())
        .toPlainText()
        .simplified();
}

QUrl DuckDuckGoProvider::extractTarget(const QUrl& url)
{
    if (!url.isValid()) {
        return {};
    }

    if (url.host().contains(QStringLiteral("duckduckgo.com"), Qt::CaseInsensitive)) {
        QUrlQuery query(url);

        const QString target = query.queryItemValue(QStringLiteral("uddg"));
        if (!target.isEmpty()) {
            return QUrl::fromEncoded(target.toUtf8());
        }
    }

    return url;
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/WikipediaProvider.hpp" <<'EOFCRAWLER'
#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QString>
#include <QStringList>

namespace efcrawler {

class WikipediaProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit WikipediaProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    enum class RequestStage {
        Idle,
        Search,
        ExternalLinks
    };

    void startSearchRequest(const QString& query);
    void startExternalLinksRequest();

    void processSearchReply(QNetworkReply* reply);
    void processExternalLinksReply(QNetworkReply* reply);

    void finish();
    void releaseActiveReply();

    [[nodiscard]] QNetworkRequest makeRequest(const QUrl& url) const;

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};

    RequestStage stage_{RequestStage::Idle};

    QStringList pageTitles_;
    QSet<QString> seenUrls_;
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/WikipediaProvider.cpp" <<'EOFCRAWLER'
#include "WikipediaProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace efcrawler {
namespace {

// Wikimedia accepts a project URL in the User-Agent. Preferring that over the
// e-mail address reduces mail harvesting; the address remains in the About box
// and the README where it belongs.
constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

} // namespace

WikipediaProvider::WikipediaProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString WikipediaProvider::name() const
{
    return QStringLiteral("Wikipedia");
}

bool WikipediaProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr || stage_ != RequestStage::Idle;
}

void WikipediaProvider::search(const QString& query)
{
    if (isBusy()) {
        return;
    }

    pageTitles_.clear();
    seenUrls_.clear();

    startSearchRequest(query);
}

void WikipediaProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void WikipediaProvider::cancel()
{
    bumpGeneration();

    // Releases the reply immediately rather than waiting for the aborted
    // finished() to arrive, so a search() on the same turn is not refused.
    releaseActiveReply();

    stage_ = RequestStage::Idle;

    // No searchFinished() here: this provider was cancelled, so the manager is
    // no longer waiting on it. Emitting would be read as the next query done.
}

QNetworkRequest WikipediaProvider::makeRequest(const QUrl& url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");
    return request;
}

void WikipediaProvider::startSearchRequest(const QString& query)
{
    QUrl url(QStringLiteral("https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("list"), QStringLiteral("search"));
    parameters.addQueryItem(QStringLiteral("srsearch"), query);
    parameters.addQueryItem(QStringLiteral("srlimit"), QStringLiteral("10"));
    parameters.addQueryItem(QStringLiteral("utf8"), QStringLiteral("1"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

    stage_ = RequestStage::Search;

    const quint64 gen = generation();
    activeReply_ = network_.get(makeRequest(url));

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            finish();
            return;
        }

        processSearchReply(reply);
        reply->deleteLater();
    });
}

void WikipediaProvider::processSearchReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        finish();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid JSON response from Wikipedia."));
        finish();
        return;
    }

    const QJsonArray results = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("search")).toArray();

    for (const QJsonValue& value : results) {
        const QString title =
            value.toObject().value(QStringLiteral("title")).toString().trimmed();

        if (title.isEmpty()) {
            continue;
        }

        pageTitles_.append(title);

        QString pageName = title;
        pageName.replace(QLatin1Char(' '), QLatin1Char('_'));

        const QUrl pageUrl(QStringLiteral("https://en.wikipedia.org/wiki/") + pageName);
        const QString canonical = pageUrl.toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = title;
        result.type = ResourceType::Web;
        result.mime = QStringLiteral("text/html");
        result.provider = name();
        result.source = QStringLiteral("en.wikipedia.org");
        result.size = ResourceClassifier::formatSize(-1);
        result.url = pageUrl;

        emit resultFound(result);
    }

    if (pageTitles_.isEmpty()) {
        finish();
        return;
    }

    startExternalLinksRequest();
}

void WikipediaProvider::startExternalLinksRequest()
{
    QUrl url(QStringLiteral("https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("prop"), QStringLiteral("extlinks"));
    parameters.addQueryItem(QStringLiteral("titles"),
                            pageTitles_.join(QLatin1Char('|')));
    // Was "max", which for a link-heavy article returns an unbounded response.
    // 100 keeps the payload predictable; full `continue` pagination is a
    // follow-up.
    parameters.addQueryItem(QStringLiteral("ellimit"), QStringLiteral("100"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

    stage_ = RequestStage::ExternalLinks;

    const quint64 gen = generation();
    activeReply_ = network_.get(makeRequest(url));

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            finish();
            return;
        }

        processExternalLinksReply(reply);
        reply->deleteLater();
    });
}

void WikipediaProvider::processExternalLinksReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        finish();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid external-link response "
                                          "from Wikipedia."));
        finish();
        return;
    }

    const QJsonArray pages = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("pages")).toArray();

    for (const QJsonValue& pageValue : pages) {
        const QJsonObject page = pageValue.toObject();
        const QString pageTitle =
            page.value(QStringLiteral("title")).toString();

        const QJsonArray links = page.value(QStringLiteral("extlinks")).toArray();

        for (const QJsonValue& linkValue : links) {
            const QString rawUrl =
                linkValue.toObject().value(QStringLiteral("url")).toString().trimmed();

            const QUrl url(rawUrl);

            if (!url.isValid()) {
                continue;
            }

            const QString scheme = url.scheme().toLower();
            if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
                continue;
            }

            const TypeInfo info = ResourceClassifier::classify(url);

            // External Web links are skipped on purpose: they are mostly
            // citations, and the article itself was already emitted above.
            // Only downloadable / media links are worth surfacing here.
            if (info.type == ResourceType::Web || info.type == ResourceType::Unknown) {
                continue;
            }

            const QString canonical =
                url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

            if (seenUrls_.contains(canonical)) {
                continue;
            }
            seenUrls_.insert(canonical);

            SearchResult result;
            result.title = url.fileName();
            if (result.title.isEmpty()) {
                result.title = QStringLiteral("%1 resource").arg(pageTitle);
            }

            result.type = info.type;
            result.mime = info.mime;
            result.downloadable = info.downloadable;
            result.streamable = info.streamable;
            result.ambiguous = info.ambiguous;
            result.provider = name();
            result.source = url.host();
            result.size = ResourceClassifier::formatSize(-1);
            result.url = url;

            emit resultFound(result);
        }
    }

    finish();
}

void WikipediaProvider::finish()
{
    activeReply_ = nullptr;
    stage_ = RequestStage::Idle;

    emit searchFinished();
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/ProviderManager.hpp" <<'EOFCRAWLER'
#pragma once

#include "SearchProvider.hpp"

#include <QList>
#include <QString>

namespace efcrawler {

// Providers are injected via addProvider() instead of being hardcoded, so the
// engine can build a second chain for media sources.
class ProviderManager final : public SearchProvider
{
    Q_OBJECT

public:
    explicit ProviderManager(QObject* parent = nullptr);

    // Takes ownership. Call before search(); order defines the fallback chain.
    void addProvider(SearchProvider* provider);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void startCurrentProvider();
    void connectProvider(SearchProvider* provider);

    QList<SearchProvider*> providers_;
    int currentProvider_{-1};
    QString currentQuery_;
    bool busy_{false};
    bool providerFailed_{false};
    QString lastFailure_;
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/ProviderManager.cpp" <<'EOFCRAWLER'
#include "ProviderManager.hpp"

namespace efcrawler {

ProviderManager::ProviderManager(QObject* parent)
    : SearchProvider(parent)
{
}

void ProviderManager::addProvider(SearchProvider* provider)
{
    if (!provider) {
        return;
    }

    provider->setParent(this);
    providers_.append(provider);
    connectProvider(provider);
}

QString ProviderManager::name() const
{
    if (currentProvider_ >= 0 && currentProvider_ < providers_.size()) {
        return providers_.at(currentProvider_)->name();
    }

    if (!providers_.isEmpty()) {
        return providers_.first()->name();
    }

    return QStringLiteral("Research Providers");
}

bool ProviderManager::isBusy() const noexcept
{
    return busy_;
}

void ProviderManager::search(const QString& query)
{
    if (busy_) {
        return;
    }

    currentQuery_ = query;
    currentProvider_ = 0;
    providerFailed_ = false;
    lastFailure_.clear();
    busy_ = true;

    startCurrentProvider();
}

void ProviderManager::cancel()
{
    if (currentProvider_ >= 0 && currentProvider_ < providers_.size()) {
        // Each child releases its own reply and bumps its own generation, so
        // no stale searchFinished() can arrive afterwards. Previously busy_
        // was dropped here while the child's activeReply_ stayed set, so the
        // next search() issued nothing and the aborted reply's completion was
        // counted as that new query finishing.
        providers_.at(currentProvider_)->cancel();
    }

    busy_ = false;
    currentProvider_ = -1;
}

void ProviderManager::startCurrentProvider()
{
    if (currentProvider_ < 0 || currentProvider_ >= providers_.size()) {
        busy_ = false;

        emit providerUnavailable(
            QStringLiteral("Research Providers"),
            lastFailure_.isEmpty()
                ? QStringLiteral("No research provider is available.")
                : lastFailure_);

        emit searchFinished();
        return;
    }

    providerFailed_ = false;

    providers_.at(currentProvider_)->search(currentQuery_);
}

void ProviderManager::connectProvider(SearchProvider* provider)
{
    connect(provider, &SearchProvider::resultFound,
            this, &SearchProvider::resultFound);

    connect(provider, &SearchProvider::providerUnavailable, this,
            [this](const QString& providerName, const QString& reason) {
                providerFailed_ = true;
                lastFailure_ = QStringLiteral("%1 unavailable — %2")
                                   .arg(providerName, reason);
            });

    connect(provider, &SearchProvider::providerError, this,
            [this](const QString& providerName, const QString& message) {
                providerFailed_ = true;
                lastFailure_ = QStringLiteral("%1 error — %2")
                                   .arg(providerName, message);
            });

    connect(provider, &SearchProvider::searchFinished, this, [this]() {
        if (!busy_) {
            return;
        }

        if (providerFailed_) {
            ++currentProvider_;

            if (currentProvider_ < providers_.size()) {
                startCurrentProvider();
                return;
            }

            busy_ = false;

            emit providerUnavailable(QStringLiteral("Research Providers"), lastFailure_);
            emit searchFinished();
            return;
        }

        busy_ = false;
        emit searchFinished();
    });
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/InternetArchiveProvider.hpp" <<'EOFCRAWLER'
#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>

namespace efcrawler {

// API-based rather than scraped, and returns `licenseurl` -- which is what
// lets LicenseGate stop guessing at licences from substrings.
class InternetArchiveProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit InternetArchiveProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

    // "audio", "movies", "image OR texts"; empty means no mediatype filter.
    void setMediatypeFilter(const QString& filter);

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    void releaseActiveReply();

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
    QString mediatypeFilter_;
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/InternetArchiveProvider.cpp" <<'EOFCRAWLER'
#include "InternetArchiveProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

namespace efcrawler {
namespace {

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

ResourceType typeForMediatype(const QString& mediatype)
{
    const QString m = mediatype.toLower();

    if (m == QLatin1String("audio"))    { return ResourceType::Audio; }
    if (m == QLatin1String("movies"))   { return ResourceType::Movie; }
    if (m == QLatin1String("image"))    { return ResourceType::Image; }
    if (m == QLatin1String("texts"))    { return ResourceType::Pdf; }
    if (m == QLatin1String("data"))     { return ResourceType::Data; }
    if (m == QLatin1String("software")) { return ResourceType::Archive; }

    return ResourceType::Media;
}

} // namespace

InternetArchiveProvider::InternetArchiveProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString InternetArchiveProvider::name() const
{
    return QStringLiteral("Internet Archive");
}

bool InternetArchiveProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void InternetArchiveProvider::setMediatypeFilter(const QString& filter)
{
    mediatypeFilter_ = filter.trimmed();
}

void InternetArchiveProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    seenUrls_.clear();

    QString qualified = QStringLiteral("(%1)").arg(query);

    if (!mediatypeFilter_.isEmpty()) {
        qualified += QStringLiteral(" AND mediatype:(%1)").arg(mediatypeFilter_);
    }

    // Built as a string rather than with QUrlQuery because the API's `fl[]`
    // parameter names contain brackets, which QUrlQuery percent-encodes.
    // VERIFY against the live endpoint on first run: if the response comes
    // back without `docs`, this encoding is the thing to look at.
    const QString full =
        QStringLiteral("https://archive.org/advancedsearch.php?q=")
        + QString::fromUtf8(QUrl::toPercentEncoding(qualified))
        + QStringLiteral("&fl[]=identifier&fl[]=title&fl[]=mediatype"
                         "&fl[]=year&fl[]=licenseurl"
                         "&rows=25&page=1&output=json");

    const QUrl url(full, QUrl::TolerantMode);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");

    const quint64 gen = generation();
    activeReply_ = network_.get(request);

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            emit searchFinished();
            return;
        }

        processReply(reply);
        reply->deleteLater();
    });
}

void InternetArchiveProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void InternetArchiveProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void InternetArchiveProvider::processReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit searchFinished();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        emit searchFinished();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid JSON response from Archive.org."));
        emit searchFinished();
        return;
    }

    const QJsonArray docs = document.object()
        .value(QStringLiteral("response")).toObject()
        .value(QStringLiteral("docs")).toArray();

    for (const QJsonValue& value : docs) {
        const QJsonObject doc = value.toObject();

        const QString identifier =
            doc.value(QStringLiteral("identifier")).toString().trimmed();

        if (identifier.isEmpty()) {
            continue;
        }

        // /details/<id> is the item page -- the Media bucket, not a file.
        // Individual files live under /download/<id>/<file> and arrive with an
        // extension, so ResourceClassifier pins those down precisely.
        const QUrl url(QStringLiteral("https://archive.org/details/") + identifier);
        const QString canonical = url.toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        const QString mediatype = doc.value(QStringLiteral("mediatype")).toString();

        SearchResult result;
        result.title = doc.value(QStringLiteral("title")).toString().trimmed();
        if (result.title.isEmpty()) {
            result.title = identifier;
        }

        result.type = typeForMediatype(mediatype);
        result.provider = name();
        result.source = QStringLiteral("archive.org");
        result.size = ResourceClassifier::formatSize(-1);
        result.url = url;

        // The whole collection is free to access; licenseurl refines how free.
        result.licenseUrl = doc.value(QStringLiteral("licenseurl")).toString();
        if (!result.licenseUrl.isEmpty()) {
            result.license = result.licenseUrl;
        }

        const int year = doc.value(QStringLiteral("year")).toInt();
        if (year > 0) {
            result.title = QStringLiteral("%1 (%2)").arg(result.title).arg(year);
        }

        emit resultFound(result);
    }

    emit searchFinished();
}

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/CommonsProvider.hpp" <<'EOFCRAWLER'
#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>

namespace efcrawler {

// The best fit for "no probe needed": one request returns MIME, byte size,
// direct file URL, and a licence short name.
class CommonsProvider final : public SearchProvider
{
    Q_OBJECT

public:
    enum class FileKind {
        Any,
        Audio,
        Video,
        Bitmap
    };

    explicit CommonsProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

    void setFileKind(FileKind kind);

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    void releaseActiveReply();

    [[nodiscard]] static QString kindToken(FileKind kind);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
    FileKind kind_{FileKind::Any};
};

} // namespace efcrawler
EOFCRAWLER

cat > "${OUT}/src/providers/CommonsProvider.cpp" <<'EOFCRAWLER'
#include "CommonsProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace efcrawler {
namespace {

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

} // namespace

CommonsProvider::CommonsProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString CommonsProvider::name() const
{
    return QStringLiteral("Wikimedia Commons");
}

bool CommonsProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void CommonsProvider::setFileKind(FileKind kind)
{
    kind_ = kind;
}

QString CommonsProvider::kindToken(FileKind kind)
{
    switch (kind) {
    case FileKind::Audio:  return QStringLiteral("audio");
    case FileKind::Video:  return QStringLiteral("video");
    case FileKind::Bitmap: return QStringLiteral("bitmap");
    case FileKind::Any:    break;
    }
    return {};
}

void CommonsProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    seenUrls_.clear();

    QString searchTerm = query;

    const QString token = kindToken(kind_);
    if (!token.isEmpty()) {
        searchTerm += QStringLiteral(" filetype:") + token;
    }

    QUrl url(QStringLiteral("https://commons.wikimedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("generator"), QStringLiteral("search"));
    parameters.addQueryItem(QStringLiteral("gsrsearch"), searchTerm);
    parameters.addQueryItem(QStringLiteral("gsrnamespace"), QStringLiteral("6"));
    parameters.addQueryItem(QStringLiteral("gsrlimit"), QStringLiteral("25"));
    parameters.addQueryItem(QStringLiteral("prop"), QStringLiteral("imageinfo"));
    parameters.addQueryItem(QStringLiteral("iiprop"),
                            QStringLiteral("url|mime|size|extmetadata"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");

    const quint64 gen = generation();
    activeReply_ = network_.get(request);

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            emit searchFinished();
            return;
        }

        processReply(reply);
        reply->deleteLater();
    });
}

void CommonsProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void CommonsProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void CommonsProvider::processReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit searchFinished();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        emit searchFinished();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid JSON response from Wikimedia Commons."));
        emit searchFinished();
        return;
    }

    const QJsonArray pages = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("pages")).toArray();

    for (const QJsonValue& pageValue : pages) {
        const QJsonObject page = pageValue.toObject();

        const QJsonArray imageInfo = page.value(QStringLiteral("imageinfo")).toArray();
        if (imageInfo.isEmpty()) {
            continue;
        }

        const QJsonObject info = imageInfo.first().toObject();

        const QUrl url(info.value(QStringLiteral("url")).toString());
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }

        const QString canonical =
            url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        const QString mime = info.value(QStringLiteral("mime")).toString();

        SearchResult result;
        result.title = page.value(QStringLiteral("title")).toString();
        result.title.remove(QStringLiteral("File:"));

        result.provider = name();
        result.source = QStringLiteral("commons.wikimedia.org");
        result.url = url;
        result.mime = mime;

        // Authoritative MIME and size -- no probe needed for these rows.
        const TypeInfo classified = ResourceClassifier::refine(
            ResourceClassifier::classify(url), mime);

        result.type = classified.type;
        result.downloadable = classified.downloadable;
        result.streamable = classified.streamable;

        // `size` arrives as a JSON number and can exceed 2 GB for a large
        // TIFF or video, so it must not go through toInt().
        const qint64 bytes =
            info.value(QStringLiteral("size")).toVariant().toLongLong();

        result.size = ResourceClassifier::formatSize(bytes);

        const QJsonObject extmetadata =
            info.value(QStringLiteral("extmetadata")).toObject();

        result.license = extmetadata.value(QStringLiteral("LicenseShortName"))
                             .toObject()
                             .value(QStringLiteral("value"))
                             .toString();

        result.licenseUrl = extmetadata.value(QStringLiteral("LicenseUrl"))
                                .toObject()
                                .value(QStringLiteral("value"))
                                .toString();

        emit resultFound(result);
    }

    emit searchFinished();
}

} // namespace efcrawler
EOFCRAWLER

# -----------------------------------------------------------------------------
#  BUILD NOTES
# -----------------------------------------------------------------------------

cat > "${OUT}/BUILD-NOTES.md" <<'EOFCRAWLER'
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
EOFCRAWLER

# -----------------------------------------------------------------------------
#  MANIFEST + TARBALL
# -----------------------------------------------------------------------------

echo "==> manifest"
( cd "${OUT}" && find . -type f | LC_ALL=C sort > MANIFEST.txt )

FILE_COUNT=$(find "${OUT}" -type f -name '*.?pp' | wc -l)

echo "==> creating ${TARBALL}"
tar -czf "${TARBALL}" "${OUT}"

echo
echo "=============================================="
echo "  Package complete"
echo "=============================================="
echo "  C++ files : ${FILE_COUNT}  (expected 21)"
echo "  Tree      : ./${OUT}/"
echo "  Tarball   : ./${TARBALL}  ($(du -h "${TARBALL}" | cut -f1))"
echo
echo "  Inspect first:"
echo "    tar -tzf ${TARBALL} | head -30"
echo
echo "  Apply:"
echo "    cd ~/path/to/efcrawler"
echo "    git checkout -b media-update"
echo "    cp -r ${OUT}/src/. src/"
echo "    # then edit CMakeLists.txt and the two lines in main.cpp"
echo "    # see ${OUT}/BUILD-NOTES.md"
echo
echo "  Rollback: git checkout ."
echo "=============================================="

if [ "${FILE_COUNT}" -ne 21 ]; then
    echo
    echo "!! WARNING: expected 21 C++ files, found ${FILE_COUNT}."
    echo "!! If you received this script from a chat response, it was probably"
    echo "!! truncated -- ask for the missing heredocs before using it."
    exit 1
fi