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
