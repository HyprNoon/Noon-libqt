#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/DeviceStatistics>

class NmController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool         wifiEnabled        READ wifiEnabled        NOTIFY networkChanged)
    Q_PROPERTY(bool         ethernet           READ ethernet           NOTIFY networkChanged)
    Q_PROPERTY(bool         wifi               READ wifi               NOTIFY networkChanged)
    Q_PROPERTY(QString      wifiStatus         READ wifiStatus         NOTIFY networkChanged)
    Q_PROPERTY(QString      networkName        READ networkName        NOTIFY networkChanged)
    Q_PROPERTY(int          networkStrength    READ networkStrength    NOTIFY networkChanged)
    Q_PROPERTY(QString      networkStrengthText READ networkStrengthText NOTIFY networkChanged)
    Q_PROPERTY(QString      downloadSpeedText  READ downloadSpeedText  NOTIFY speedChanged)
    Q_PROPERTY(QString      uploadSpeedText    READ uploadSpeedText    NOTIFY speedChanged)
    Q_PROPERTY(QString      materialSymbol     READ materialSymbol     NOTIFY networkChanged)
    Q_PROPERTY(QString      ipAddress          READ ipAddress          NOTIFY networkChanged)
    Q_PROPERTY(QVariantList wifiNetworks       READ wifiNetworks       NOTIFY networkChanged)
    Q_PROPERTY(int          updateInterval     READ updateInterval     WRITE setUpdateInterval NOTIFY updateIntervalChanged)

public:
    explicit NmController(QObject* parent = nullptr);

    bool         wifiEnabled()         const;
    bool         ethernet()            const;
    bool         wifi()                const;
    QString      wifiStatus()          const;
    QString      networkName()         const;
    int          networkStrength()     const;
    QString      networkStrengthText() const;
    QString      downloadSpeedText()   const;
    QString      uploadSpeedText()     const;
    QString      materialSymbol()      const;
    QString      ipAddress()           const;
    QVariantList wifiNetworks()        const;
    int          updateInterval()      const;
    void         setUpdateInterval(int ms);

    Q_INVOKABLE void toggleWifi();
    Q_INVOKABLE void enableWifi(bool enabled);
    Q_INVOKABLE void rescanWifi();
    Q_INVOKABLE void connectToWifiNetwork(const QString& ssid, const QString& password = {});
    Q_INVOKABLE void disconnectWifiNetwork();
    Q_INVOKABLE void forgetWifiNetwork(const QString& ssid);

signals:
    void networkChanged();
    void speedChanged();
    void updateIntervalChanged();

private:
    NetworkManager::WirelessDevice::Ptr wirelessDevice()  const;
    NetworkManager::Device::Ptr         activeDevice()    const;
    void                                hookDevice(const QString& uni);
    void                                rewireStats();
    void                                updateSpeed();
    static QString                      fmtSpeed(qreal bps);

    QSet<QString>                           m_hookedDevices;
    QTimer                                  m_speedTimer;
    NetworkManager::DeviceStatistics::Ptr   m_devStats;
    QString                                 m_statsDeviceUni;
    int                                     m_updateInterval = 1000;
    qreal                                   m_dl      = 0;
    qreal                                   m_ul      = 0;
    qulonglong                              m_lastRx  = 0;
    qulonglong                              m_lastTx  = 0;
    bool                                    m_seeded       = false;
    bool                                    m_statsUpdated = false;
};
