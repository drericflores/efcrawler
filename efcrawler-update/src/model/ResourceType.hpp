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
