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
