#pragma once

#include <QMetaType>
#include <QString>
#include <QUrl>

namespace efcrawler {

struct SearchResult
{
    QString title;
    QString type;
    QString source;
    QString size;
    QString access;
    QUrl url;
};

} // namespace efcrawler

Q_DECLARE_METATYPE(efcrawler::SearchResult)
