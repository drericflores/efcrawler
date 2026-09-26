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
