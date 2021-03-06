#ifndef APPLICATIONSMODEL_H
#define APPLICATIONSMODEL_H

#include <QAbstractListModel>
#include <QDBusObjectPath>

class QDBusInterface;

class AppItem: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString storeId MEMBER m_storeId CONSTANT)
    Q_PROPERTY(QString uuid MEMBER m_uuid CONSTANT)
    Q_PROPERTY(QString name MEMBER m_name CONSTANT)
    Q_PROPERTY(QString icon MEMBER m_icon CONSTANT)
    Q_PROPERTY(QString vendor MEMBER m_vendor NOTIFY vendorChanged)
    Q_PROPERTY(QString version MEMBER m_version NOTIFY versionChanged)
    Q_PROPERTY(QString description MEMBER m_description CONSTANT)
    Q_PROPERTY(int hearts MEMBER m_hearts CONSTANT)
    Q_PROPERTY(QStringList screenshotImages MEMBER m_screenshotImages CONSTANT)
    Q_PROPERTY(QString headerImage READ headerImage NOTIFY headerImageChanged)
    Q_PROPERTY(QString category MEMBER m_category CONSTANT)
    Q_PROPERTY(QString collection MEMBER m_collection CONSTANT)
    Q_PROPERTY(bool isWatchFace MEMBER m_isWatchFace NOTIFY isWatchFaceChanged)
    Q_PROPERTY(bool isSystemApp MEMBER m_isSystemApp CONSTANT)
    Q_PROPERTY(bool hasSettings MEMBER m_hasSettings CONSTANT)
    Q_PROPERTY(bool companion MEMBER m_companion CONSTANT)

    Q_PROPERTY(QString groupId READ groupId NOTIFY groupIdChanged)
    Q_PROPERTY(QString groupKind READ groupKind NOTIFY groupKindChanged)

    Q_PROPERTY(QVariantList changeLog READ changeLog NOTIFY changeLogChanged)
    Q_PROPERTY(QString latest READ latest WRITE setLatest NOTIFY latestChanged)
    Q_PROPERTY(QVariantMap compatibility READ compatibility NOTIFY compatibilityChanged)

public:
    AppItem(QObject *parent = 0);

    QString storeId() const;
    QString uuid() const;
    QString name() const;
    QString icon() const;
    QString vendor() const;
    QString version() const;
    QString description() const;
    int hearts() const;
    QStringList screenshotImages() const;
    QString headerImage() const;
    bool isWatchFace() const;
    bool isSystemApp() const;
    bool hasSettings() const;
    bool companion() const;
    QString category() const;
    QString collection() const;

    enum GroupKind {
        GroupCollection,
        GroupCategory
    };
    GroupKind groupKind() const;
    QString groupId(const GroupKind kind) const;
    QString groupId() const;

    void setStoreId(const QString &storeId);
    void setUuid(const QString &uuid);
    void setName(const QString &name);
    void setIcon(const QString &icon);
    void setVendor(const QString &vendor);
    void setVersion(const QString &version);
    void setDescription(const QString &description);
    void setHearts(int hearts);
    void setCategory(const QString &category);
    void setCollection(const QString &collection);
    void setScreenshotImages(const QStringList &screenshotImages);
    void setHeaderImage(const QString &headerImage);
    void setIsWatchFace(bool isWatchFace);
    void setIsSystemApp(bool isSystemApp);
    void setHasSettings(bool hasSettings);
    void setCompanion(bool companion);

    // For grouping in lists, e.g. by collection
    void setGroupId(const QString &groupId, const GroupKind kind = GroupCollection);
    void setGroupKind(const GroupKind kind);

    // For installed app upgrade
    QVariantList changeLog() const;
    QVariantMap compatibility() const;
    QString latest() const;
    void setLatest(const QString version);
    void setCompatibility(const QVariantMap &map);
    void setChangeLog(const QVariantList &log);

signals:
    void versionChanged();
    void vendorChanged();
    void headerImageChanged();
    void isWatchFaceChanged();
    void groupIdChanged();
    void groupKindChanged();
    void changeLogChanged();
    void latestChanged();
    void compatibilityChanged();

private:
    QString m_storeId;
    QString m_uuid;
    QString m_name;
    QString m_icon;
    QString m_vendor;
    QString m_version;
    QString m_description;
    int m_hearts = 0;
    QString m_category;
    QString m_collection;
    QStringList m_screenshotImages;
    bool m_isWatchFace = false;
    bool m_isSystemApp = false;
    bool m_hasSettings = false;
    bool m_companion = false;

    QHash<GroupKind,QString> m_groupIds;
    GroupKind m_groupKind;

    QString m_headerImage;

    QVariantList m_changeLog;
    QString m_latest;
    QVariantMap m_compatibility;
};

class ApplicationsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList links READ links NOTIFY linksChanged)

public:
    enum Roles {
        RoleStoreId,
        RoleUuid,
        RoleName,
        RoleIcon,
        RoleVendor,
        RoleVersion,
        RoleIsWatchFace,
        RoleIsSystemApp,
        RoleHasSettings,
        RoleDescription,
        RoleHearts,
        RoleCategory,
        RoleGroupId,
        RoleCollection,
        RoleHasCompanion,
        RoleChangeLog,
        RoleLatest,
        RoleCompatibility
    };

    ApplicationsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void insert(AppItem *item);
    void insertGroup(const QString &id, const QString &name, const QString &link, const QString &icon = "");

    Q_INVOKABLE AppItem* get(int index) const;
    AppItem* findByStoreId(const QString &storeId) const;
    AppItem* findByUuid(const QString &uuid) const;
    Q_INVOKABLE bool contains(const QString &storeId) const;
    int indexOf(AppItem *item) const;

    Q_INVOKABLE QString groupName(const QString &groupId) const;
    Q_INVOKABLE QString groupLink(const QString &groupId) const;
    Q_INVOKABLE QString groupIcon(const QString &groupId) const;

    QStringList links() const;
    Q_INVOKABLE QString linkName(const QString &link) const;
    void addLink(const QString &link, const QString &name);

    // Added to enable pagination in searches. The QML page decides the correct call by reading the search page.
    Q_INVOKABLE QString linkQuery(const QString &link) const;
    Q_INVOKABLE int linkPage(const QString &link) const;
    void addLink(const QString &link, const QString &name, const QString &query, const int page);

    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void commitMove();

signals:
    void linksChanged();
    void appsSorted();
    void changed();

private:
    QList<AppItem*> m_apps;
    QHash<QString, QString> m_groupNames;
    QHash<QString, QString> m_groupLinks;
    QHash<QString, QString> m_groupIcons;
    QStringList m_links;
    QHash<QString, QString> m_linkNames;
    QHash<QString, QString> m_linkQueries;
    QHash<QString, int> m_linkPages;
};

#endif // APPLICATIONSMODEL_H
