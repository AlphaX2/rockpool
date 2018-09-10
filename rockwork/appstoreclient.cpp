#include "appstoreclient.h"
#include "applicationsmodel.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <libintl.h>

/* Known params for pebble api
    query.addQueryItem("offset", QString::number(offset));
    query.addQueryItem("limit", QString::number(limit));
    query.addQueryItem("image_ratio", "1"); // Not sure yet what this does
    query.addQueryItem("filter_hardware", "true");
    query.addQueryItem("firmware_version", "3");
    query.addQueryItem("hardware", hardwarePlatform);
    query.addQueryItem("platform", "all");
*/

AppStoreClient::AppStoreClient(QObject *parent):
    QObject(parent),
    m_nam(new QNetworkAccessManager(this)),
    m_model(new ApplicationsModel(this))
{
}

ApplicationsModel *AppStoreClient::model() const
{
    return (f_model==0)? m_model : f_model;
}

void AppStoreClient::setModel(ApplicationsModel *p_model)
{
    f_model = p_model;
    emit modelChanged();
}

int AppStoreClient::limit() const
{
    return m_limit;
}

void AppStoreClient::setLimit(int limit)
{
    m_limit = limit;
    emit limitChanged();
}

QString AppStoreClient::hardwarePlatform() const
{
    return m_hardwarePlatform;
}

void AppStoreClient::setHardwarePlatform(const QString &hardwarePlatform)
{
    m_hardwarePlatform = hardwarePlatform;
    emit hardwarePlatformChanged();
}
void AppStoreClient::setEnableCategories(const bool enable)
{
    m_enableCategories = enable;
    emit enableCategoriesChanged();
}

bool AppStoreClient::enableCategories() const
{
    return m_enableCategories;
}

bool AppStoreClient::busy() const
{
    return m_busy;
}

void AppStoreClient::fetchHome(Type type)
{
    m_model->clear();
    setBusy(true);

    QUrlQuery query;
    query.addQueryItem("firmware_version", "3");
    if (!m_hardwarePlatform.isEmpty()) {
        query.addQueryItem("hardware", m_hardwarePlatform);
        query.addQueryItem("filter_hardware", "true");
    }

    QString url;
    if (type == TypeWatchapp) {
        url = "https://appstore-api.rebble.io/api/v1/home/apps";
    } else {
        url = "https://appstore-api.rebble.io/api/v1/home/watchfaces";
    }
    QUrl storeUrl(url);
    storeUrl.setQuery(query);
    QNetworkRequest request(storeUrl);

    qDebug() << "fetching home" << storeUrl.toString();
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        QVariantMap resultMap = jsonDoc.toVariant().toMap();

        QHash<QString, QStringList> collections;
        foreach (const QVariant &entry, resultMap.value("collections").toList()) {
            QStringList appIds;
            foreach (const QVariant &appId, entry.toMap().value("application_ids").toList()) {
                appIds << appId.toString();
            }
            QString slug = entry.toMap().value("slug").toString();
            collections[slug] = appIds;
            m_model->insertGroup(slug, entry.toMap().value("name").toString(), entry.toMap().value("links").toMap().value("apps").toString());
        }

        QHash<QString, QString> categoryNames;
        foreach (const QVariant &entry, resultMap.value("categories").toList()) {
            categoryNames[entry.toMap().value("id").toString()] = entry.toMap().value("name").toString();
            if(m_enableCategories) {
                m_model->insertGroup(entry.toMap().value("id").toString(),
                                     entry.toMap().value("name").toString(),
                                     entry.toMap().value("links").toMap().value("apps").toString(),
                                     entry.toMap().value("icon").toMap().value("88x88").toString());
            }
        }

        foreach (const QVariant &entry, jsonDoc.toVariant().toMap().value("applications").toList()) {
            AppItem* item = parseAppItem(entry.toMap());
            foreach (const QString &collection, collections.keys()) {
                if (collections.value(collection).contains(item->storeId())) {
                    item->setGroupId(collection);
                    item->setCollection(m_model->groupName(collection));
                    break;
                }
            }
            item->setCategory(categoryNames.value(entry.toMap().value("category_id").toString()));

            if(m_enableCategories) {
                item->setGroupId(entry.toMap().value("category_id").toString(),AppItem::GroupCategory);
            }
            //qDebug() << "have entry" << item->name() << item->groupId() << item->companion() << item->collection();

            /* Let's just disable ability to install, and give user ability to filter
            if (item->groupId().isEmpty() || item->companion()) {
                // Skip items that we couldn't match to a collection
                // Also skip apps that need a companion
                delete item;
                continue;
            }
            */
            m_model->insert(item);
        }
        setBusy(false);
    });


}

void AppStoreClient::fetchLink(const QString &link)
{
    m_model->clear();
    setBusy(true);

    QUrl storeUrl(link);
    QUrlQuery query(storeUrl);
    // Navigation buttons are supplied based on actual navigation availability
    // Limit is preserved in links as long as it differs from default(20).
    query.removeQueryItem("limit");
    query.addQueryItem("limit", QString::number(m_limit));
    if (!query.hasQueryItem("hardware")) {
        query.addQueryItem("hardware", m_hardwarePlatform);
        query.addQueryItem("filter_hardware", "true");
    }
    storeUrl.setQuery(query);
    QNetworkRequest request(storeUrl);
    qDebug() << "fetching link" << request.url();

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply, storeUrl]() {
        qDebug() << "fetch reply";
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        QVariantMap resultMap = jsonDoc.toVariant().toMap();

        foreach (const QVariant &entry, resultMap.value("data").toList()) {
            AppItem *item = parseAppItem(entry.toMap());
            qDebug() << "have entry" << item->name() << item->groupId() << item->companion() << item->category();
            // We filter apps in ModelFilter now. Since we didn't reqeuest additional apps to align for those
            // filtered out - it doesn't change end result. maybe slightly increases memory footprint.
            m_model->insert(item);
        }
        // Links should be built independently, otherwise we cannot go Previous from last page (no Next).
        int currentOffset = resultMap.value("offset").toInt();
        if (currentOffset > 0) {
            QUrl previousLink(storeUrl);
            QUrlQuery query(previousLink);
            query.removeQueryItem("offset");
            query.addQueryItem("offset", QString::number(qMax(0, currentOffset - m_limit)));
            previousLink.setQuery(query);
            m_model->addLink(previousLink.toString(), gettext("Previous"));
        }
        // NextLink is provided when there's more to fetch, no need for *haveMore* hack.
        if (resultMap.contains("links") && resultMap.value("links").toMap().contains("nextPage") &&
                !resultMap.value("links").toMap().value("nextPage").isNull()) {
            QString nextLink = resultMap.value("links").toMap().value("nextPage").toString();
            m_model->addLink(nextLink, gettext("Next"));
        }
        setBusy(false);
    });

}

void AppStoreClient::fetchAppDetails(const QString &appId)
{
    QUrl url("https://appstore-api.rebble.io/api/v1/apps/id/" + appId);
    QUrlQuery query;
    if (!m_hardwarePlatform.isEmpty()) {
        query.addQueryItem("hardware", m_hardwarePlatform);
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply * reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply, appId]() {
        reply->deleteLater();
        AppItem *item = model()->findByStoreId(appId);
        if (!item) {
            qWarning() << "Can't find item with id" << appId;
            return;
        }
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        if(jsonDoc.isEmpty()) {
            qDebug() << "Not a store app" << appId << "or problem with store connection" << reply->errorString();
            return;
        } else
            qDebug() << "Attempting to parse for" << appId << item->category() << "cat";
        QVariantMap replyMap = jsonDoc.toVariant().toMap().value("data").toList().first().toMap();
        if(!item->category().isNull()) { // This is store item, category is always set
            item->setVersion(replyMap.value("latest_release").toMap().value("version").toString());
            if (replyMap.contains("header_images") && replyMap.value("header_images").toList().count() > 0) {
                item->setHeaderImage(replyMap.value("header_images").toList().first().toMap().value("orig").toString());
            }
            item->setVendor(replyMap.value("author").toString());
            item->setIsWatchFace(replyMap.value("type").toString() == "watchface");
        } else { // This is phone item (installedApps) - category not preserved
            qDebug() << "Fetching data for app" << appId;// << jsonDoc.toJson(QJsonDocument::Indented);
            QVariantMap flatMap,cmpMap = replyMap.value("compatibility").toMap();
            foreach(const QString &key, cmpMap.keys()) {
                if(cmpMap.value(key).toMap().value("supported").toBool()) {
                    //qDebug() << "Adding compatibility for" << appId << key;
                    if(cmpMap.value(key).toMap().contains("firmware")) {
                        flatMap.insert(key,cmpMap.value(key).toMap().value("firmware").toMap().value("major").toInt());
                    } else {
                        flatMap.insert(key,1);
                    }
                }
            }
            item->setLatest(replyMap.value("latest_release").toMap().value("version").toString());
            item->setChangeLog(replyMap.value("changelog").toList());
            item->setCompanion(!replyMap.value("companions").toMap().value("android").isNull() || !replyMap.value("companions").toMap().value("ios").isNull());
            item->setCompatibility(flatMap);
            QModelIndex mi = model()->index(model()->indexOf(item));
            emit model()->dataChanged(mi,mi);
        }
    });
}

void AppStoreClient::search(const QString &searchString, Type type)
{
    m_model->clear();
    setBusy(true);

    QUrl url("https://7683ow76eq-dsn.algolia.net/1/indexes/rebble-appstore-production/query");
    QUrlQuery query;
    query.addQueryItem("x-algolia-api-key", "252f4938082b8693a8a9fc0157d1d24f");
    query.addQueryItem("x-algolia-application-id", "7683OW76EQ");
    url.setQuery(query);

    QString filter = "watchapp";
    if (type == TypeWatchface) {
        filter = "watchface";
    }

    QString pluralFilter = filter + "s";
    QString params = QString("query=%1&hitsPerPage=30&page=0&tagFilters=(%2)&analyticsTags=product-variant-time,%3,%4,appstore-search")
        .arg(searchString).arg(filter).arg(m_hardwarePlatform).arg(pluralFilter);
    QJsonObject json;
    json.insert("params", params);
    QJsonDocument doc;
    doc.setObject(json);
    QByteArray body = doc.toJson();

    QNetworkRequest request(url);
    qDebug() << "Search query:" << url << params;
    QNetworkReply *reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        m_model->clear();
        setBusy(false);

        reply->deleteLater();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());

        QVariantMap resultMap = jsonDoc.toVariant().toMap();
        foreach (const QVariant &entry, resultMap.value("hits").toList()) {
            AppItem *item = parseAppItem(entry.toMap());
            m_model->insert(item);
//            qDebug() << "have item" << item->storeId() << item->name() << item->icon();
        }
        qDebug() << "Found" << m_model->rowCount() << "items";
    });
}

AppItem* AppStoreClient::parseAppItem(const QVariantMap &map)
{
    AppItem *item = new AppItem();
    item->setStoreId(map.value("id").toString());
    item->setName(map.value("title").toString());
    if (!map.value("list_image").toString().isEmpty()) {
        item->setIcon(map.value("list_image").toString());
    } else {
        item->setIcon(map.value("list_image").toMap().value("144x144").toString());
    }
    item->setDescription(map.value("description").toString());
    item->setHearts(map.value("hearts").toInt());
    item->setCategory(map.value("category_name").toString());
    item->setCompanion(!map.value("companions").toMap().value("android").isNull() || !map.value("companions").toMap().value("ios").isNull());

    QVariantList screenshotsList = map.value("screenshot_images").toList();
    // try to get more hardware specific screenshots. The store search keeps them in a subgroup.
    if (map.contains("asset_collections")) {
        foreach (const QVariant &assetCollection, map.value("asset_collections").toList()) {
            if (assetCollection.toMap().value("hardware_platform").toString() == m_hardwarePlatform) {
                screenshotsList = assetCollection.toMap().value("screenshots").toList();
                break;
            }
        }
    }
    QStringList screenshotImages;
    foreach (const QVariant &screenshotItem, screenshotsList) {
        if (!screenshotItem.toString().isEmpty()) {
            screenshotImages << screenshotItem.toString();
        } else if (screenshotItem.toMap().count() > 0) {
            screenshotImages << screenshotItem.toMap().first().toString();
        }
    }
    item->setScreenshotImages(screenshotImages);
//    qDebug() << "setting screenshot images" << item->screenshotImages();

    // The search seems to return references to invalid icon images. if we detect that, we'll replace it with a screenshot
    if (item->icon().contains("aOUhkV1R1uCqCVkKY5Dv") && !item->screenshotImages().isEmpty()) {
        item->setIcon(item->screenshotImages().first());
    }

    return item;
}

void AppStoreClient::setBusy(bool busy)
{
    m_busy = busy;
    emit busyChanged();
}

