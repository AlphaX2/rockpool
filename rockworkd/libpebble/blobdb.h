#ifndef BLOBDB_H
#define BLOBDB_H

#include "watchconnection.h"
#include "pebble.h"

#include <QObject>

class AppMetadata;
class TimelineItem;

class BlobDB : public QObject
{
    Q_OBJECT
public:
    enum BlobDBId {
        BlobDBIdTest = 0,
        BlobDBIdPin = 1,
        BlobDBIdApp = 2,
        BlobDBIdReminder = 3,
        BlobDBIdNotification = 4,
        BlobDBIdWeatherData = 5,
        blobDBIdSendTextData = 6,
        BlobDBIdAppSettings = 7, // this is rather HealthAppSettings
        BlobDBIdContacts = 8,
        BlobDBIdAppConfigs = 9 // Config Data References
    };
    enum Operation {
        OperationInsert = 0x01,
        OperationDelete = 0x04,
        OperationClear = 0x05,
        OperationNotify = 0x8,
        OperationInvalid = 0xff
    };

    enum Response {
        ResponseSuccess = 0x00, // ACK
        ResponseError = 0x01    // NACK
    };

    enum Status {
        StatusIgnore = 0x0,
        StatusSuccess = 0x1,
        StatusFailure = 0x2,
        StatusInvalOp = 0x3,
        StatusInvalId = 0x4,
        StatusInvalData = 0x5,
        StatusNoSuchKey = 0x6,
        StatusDbIsFull = 0x7,
        StatusDbIsStale = 0x8
    };

    explicit BlobDB(Pebble *pebble, WatchConnection *connection);

    void clearApps();
    void insertAppMetaData(const AppInfo &info, const bool force=false);
    void removeApp(const AppInfo &info);

    void insert(BlobDBId database, const BlobDbItem &item);
    void remove(BlobDBId database, const QByteArray &key);
    void clear(BlobDBId database);

    void setHealthParams(const HealthParams &healthParams);
    void setUnits(bool imperial);

private slots:
    void blobCommandReply(const QByteArray &data);
    void blobUpdateNotify(const QByteArray &data);
    void sendNext();

signals:
    void appInserted(const QUuid &uuid);
    void blobCommandResult(BlobDBId db, Operation cmd, const QByteArray &key, Status ack);
    void notifyTimeline(const QDateTime &ts, const QUuid &key, const TimelineItem &val);

private:
    static inline quint16 generateToken();
    AppMetadata appInfoToMetadata(const AppInfo &info, HardwarePlatform hardwarePlatform);

private:

    class BlobCommand: public PebblePacket
    {
    public:
        BlobDB::Operation m_command; // quint8
        quint16 m_token;
        BlobDB::BlobDBId m_database;
        quint32 m_timestamp;

        QByteArray m_key;
        QByteArray m_value;

        QByteArray serialize() const override;
        bool deserialize(const QByteArray &data) override;
    };

    Pebble *m_pebble;
    WatchConnection *m_connection;

    HealthParams m_healthParams;

    BlobCommand *m_currentCommand = nullptr;
    QList<BlobCommand*> m_commandQueue;

    QString m_blobDBStoragePath;
};

#endif // BLOBDB_H
