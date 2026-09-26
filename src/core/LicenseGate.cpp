#include "LicenseGate.hpp"

#include <QStringList>

#include <algorithm>

namespace efcrawler {
namespace {

// Strong signals: these genuinely imply free / open / public-domain access.
const QStringList kStrongFreeHosts = {
    QStringLiteral("gutenberg.org"),      QStringLiteral("commons.wikimedia.org"),
    QStringLiteral("openverse.org"),      QStringLiteral("doaj.org"),
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
    QStringLiteral("creativecommons.org/licenses/"),
    QStringLiteral("creativecommons.org/publicdomain/zero/"),
    QStringLiteral("creativecommons.org/publicdomain/mark/"),
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

    // A file extension does not establish licensing; host and path signals do.
    if (commercialHost || commercialPath) {
        return Access::Commercial;
    }

    const bool strongFree =
        std::any_of(kStrongFreeHosts.cbegin(), kStrongFreeHosts.cend(),
                    [&host](const QString& domain) {
                        return host == domain ||
                               host.endsWith(QLatin1Char('.') + domain);
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
