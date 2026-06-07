#include "NetworkManager.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessSecuritySetting>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Ipv6Setting>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/Settings>

NetworkManager::WirelessDevice::Ptr NmController::wirelessDevice() const {
    NetworkManager::WirelessDevice::Ptr fallback;
    for (auto& d : NetworkManager::networkInterfaces()) {
        if (!d || d->type() != NetworkManager::Device::Wifi) continue;
        auto wd = d.objectCast<NetworkManager::WirelessDevice>();
        if (!wd) continue;
        if (d->state() == NetworkManager::Device::Activated) return wd;
        if (!fallback) fallback = wd;
    }
    return fallback;
}

NetworkManager::Device::Ptr NmController::activeDevice() const {
    for (auto& ac : NetworkManager::activeConnections()) {
        if (!ac) continue;
        for (const QString& uni : ac->devices()) {
            auto d = NetworkManager::findNetworkInterface(uni);
            if (!d || d->state() != NetworkManager::Device::Activated) continue;
            const auto t = d->type();
            if (t != NetworkManager::Device::Wifi && t != NetworkManager::Device::Ethernet) continue;
            return d;
        }
    }
    return {};
}

static NetworkManager::WirelessSetting::Ptr wirelessSetting(const NetworkManager::Connection::Ptr& c) {
    if (!c) return {};
    auto s = c->settings();
    if (!s || s->connectionType() != NetworkManager::ConnectionSettings::Wireless) return {};
    return s->setting(NetworkManager::Setting::Wireless)
            .staticCast<NetworkManager::WirelessSetting>();
}

QString NmController::fmtSpeed(qreal bps) {
    static const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s" };
    int i = 0;
    while (bps >= 1024.0 && i < 3) { bps /= 1024.0; ++i; }
    return QString::number(bps, 'f', 1) + QLatin1Char(' ') + QLatin1String(units[i]);
}

NmController::NmController(QObject* parent) : QObject(parent) {
    connect(&m_speedTimer, &QTimer::timeout, this, &NmController::updateSpeed);

    QTimer::singleShot(0, this, [this] {
        auto* nm = NetworkManager::notifier();

        connect(nm, &NetworkManager::Notifier::activeConnectionsChanged, this, [this] {
            rewireStats();
            emit networkChanged();
        }, Qt::QueuedConnection);
        connect(nm, &NetworkManager::Notifier::deviceAdded, this, [this](const QString& uni) {
            hookDevice(uni);
            emit networkChanged();
        }, Qt::QueuedConnection);
        connect(nm, &NetworkManager::Notifier::deviceRemoved, this, [this] {
            rewireStats();
            emit networkChanged();
        }, Qt::QueuedConnection);
        connect(nm, &NetworkManager::Notifier::wirelessEnabledChanged, this, [this] { emit networkChanged(); }, Qt::QueuedConnection);
        connect(nm, &NetworkManager::Notifier::connectivityChanged,    this, [this] { emit networkChanged(); }, Qt::QueuedConnection);

        for (auto& d : NetworkManager::networkInterfaces())
            if (d) hookDevice(d->uni());

        auto* sn = NetworkManager::settingsNotifier();
        connect(sn, &NetworkManager::SettingsNotifier::connectionAdded,   this, [this] { emit networkChanged(); }, Qt::QueuedConnection);
        connect(sn, &NetworkManager::SettingsNotifier::connectionRemoved, this, [this] { emit networkChanged(); }, Qt::QueuedConnection);

        m_speedTimer.start(m_updateInterval);
        rewireStats();
    });
}

void NmController::hookDevice(const QString& uni) {
    if (m_hookedDevices.contains(uni)) return;

    auto d = NetworkManager::findNetworkInterface(uni);
    if (!d) return;

    m_hookedDevices.insert(uni);

    connect(d.get(), &NetworkManager::Device::stateChanged, this,
        [this](NetworkManager::Device::State, NetworkManager::Device::State, NetworkManager::Device::StateChangeReason) {
            rewireStats();
            emit networkChanged();
        }, Qt::QueuedConnection);

    connect(d.get(), &NetworkManager::Device::ipV4ConfigChanged, this,
        [this] { emit networkChanged(); }, Qt::QueuedConnection);

    connect(d.get(), &QObject::destroyed, this,
        [this, uni] { m_hookedDevices.remove(uni); });

    auto wd = d.objectCast<NetworkManager::WirelessDevice>();
    if (!wd) return;

    connect(wd.get(), &NetworkManager::WirelessDevice::activeAccessPointChanged, this,
        [this] { emit networkChanged(); }, Qt::QueuedConnection);
    connect(wd.get(), &NetworkManager::WirelessDevice::networkAppeared, this,
        [this] { emit networkChanged(); }, Qt::QueuedConnection);
    connect(wd.get(), &NetworkManager::WirelessDevice::networkDisappeared, this,
        [this] { emit networkChanged(); }, Qt::QueuedConnection);
}

void NmController::rewireStats() {
    auto d = activeDevice();
    const QString uni = d ? d->uni() : QString();
    if (uni == m_statsDeviceUni) return;

    m_statsDeviceUni = uni;
    m_devStats.reset();
    m_seeded = false;
    m_dl = m_ul = 0;

    if (!d) return;
    m_devStats = d->deviceStatistics();
    if (!m_devStats) return;

    auto* statsPtr = m_devStats.get();
    m_devStats->setRefreshRateMs(m_updateInterval);
    connect(m_devStats.get(), &NetworkManager::DeviceStatistics::rxBytesChanged, this, [this, statsPtr](qulonglong rx) {
        if (m_devStats.get() != statsPtr) return;
        const qulonglong tx = m_devStats->txBytes();
        m_dl = (m_seeded && rx >= m_lastRx) ? qreal(rx - m_lastRx) : 0;
        m_ul = (m_seeded && tx >= m_lastTx) ? qreal(tx - m_lastTx) : 0;
        m_lastRx = rx;
        m_lastTx = tx;
        m_seeded       = true;
        m_statsUpdated = true;
        emit speedChanged();
    });
}

void NmController::updateSpeed() {
    if (!m_statsUpdated) {
        m_dl = m_ul = 0;
        emit speedChanged();
    }
    m_statsUpdated = false;
}

bool NmController::wifiEnabled() const { return NetworkManager::isWirelessEnabled(); }

bool NmController::ethernet() const {
    for (auto& ac : NetworkManager::activeConnections())
        if (ac && ac->type() == NetworkManager::ConnectionSettings::Wired) return true;
    return false;
}

bool NmController::wifi() const {
    for (auto& ac : NetworkManager::activeConnections())
        if (ac && ac->type() == NetworkManager::ConnectionSettings::Wireless) return true;
    return false;
}

QString NmController::wifiStatus() const {
    auto wd = wirelessDevice();
    if (!wd) return QStringLiteral("disconnected");
    switch (wd->state()) {
        case NetworkManager::Device::Activated:
            return QStringLiteral("connected");
        case NetworkManager::Device::Preparing:
        case NetworkManager::Device::ConfiguringHardware:
        case NetworkManager::Device::NeedAuth:
        case NetworkManager::Device::ConfiguringIp:
        case NetworkManager::Device::CheckingIp:
        case NetworkManager::Device::WaitingForSecondaries:
            return QStringLiteral("connecting");
        default:
            return QStringLiteral("disconnected");
    }
}

QString NmController::networkName() const {
    auto wd = wirelessDevice();
    if (wd) {
        auto ap = wd->activeAccessPoint();
        if (ap && !ap->ssid().isEmpty()) return ap->ssid();
    }
    for (auto& ac : NetworkManager::activeConnections())
        if (ac && !ac->id().isEmpty()) return ac->id();
    return {};
}

int NmController::networkStrength() const {
    auto wd = wirelessDevice();
    auto ap = wd ? wd->activeAccessPoint() : nullptr;
    return ap ? ap->signalStrength() : 0;
}

QString NmController::networkStrengthText() const {
    return QString::number(networkStrength()) + QLatin1Char('%');
}

QString NmController::downloadSpeedText() const { return fmtSpeed(m_dl); }
QString NmController::uploadSpeedText()   const { return fmtSpeed(m_ul); }

QString NmController::materialSymbol() const {
    if (ethernet())                                   return QStringLiteral("lan");
    if (!wifiEnabled())                               return QStringLiteral("signal_wifi_off");
    const QString s = wifiStatus();
    if (s == QLatin1String("connecting"))             return QStringLiteral("signal_wifi_statusbar_not_connected");
    if (s != QLatin1String("connected"))              return QStringLiteral("wifi_find");
    const int sig = networkStrength();
    if (sig < 25)                                     return QStringLiteral("signal_wifi_0_bar");
    if (sig < 50)                                     return QStringLiteral("network_wifi_1_bar");
    if (sig < 75)                                     return QStringLiteral("network_wifi_2_bar");
    if (sig < 90)                                     return QStringLiteral("network_wifi_3_bar");
    return QStringLiteral("signal_wifi_4_bar");
}

QString NmController::ipAddress() const {
    for (auto& ac : NetworkManager::activeConnections()) {
        if (!ac) continue;
        for (const QString& uni : ac->devices()) {
            auto d = NetworkManager::findNetworkInterface(uni);
            if (!d || d->state() != NetworkManager::Device::Activated) continue;
            if (d->type() == NetworkManager::Device::Loopback) continue;
            const auto addrs = d->ipV4Config().addresses();
            if (addrs.isEmpty()) continue;
            const QString ip = addrs.first().ip().toString();
            if (!ip.isEmpty() && ip != QLatin1String("0.0.0.0")) return ip;
        }
    }
    return QStringLiteral("0.0.0.0");
}

QVariantList NmController::wifiNetworks() const {
    auto wd = wirelessDevice();
    if (!wd) return {};

    QSet<QString> saved;
    for (auto& c : NetworkManager::listConnections()) {
        auto ws = wirelessSetting(c);
        if (ws && !ws->ssid().isEmpty())
            saved.insert(QString::fromUtf8(ws->ssid()));
    }

    const QString activeSsid = wd->activeAccessPoint()
                               ? wd->activeAccessPoint()->ssid()
                               : QString();

    auto networks = wd->networks();
    networks.erase(std::remove_if(networks.begin(), networks.end(),
        [](const auto& n) { return !n; }), networks.end());
    const auto isActive = [&](const NetworkManager::WirelessNetwork::Ptr& n) {
        return n && n->ssid() == activeSsid;
    };
    std::sort(networks.begin(), networks.end(),
        [&](const NetworkManager::WirelessNetwork::Ptr& a,
            const NetworkManager::WirelessNetwork::Ptr& b) {
            const bool aActive = isActive(a), bActive = isActive(b);
            if (aActive != bActive) return aActive;
            return a->signalStrength() > b->signalStrength();
        });

    QVariantList out;
    for (auto& net : networks) {
        const QString ssid = net->ssid();
        const int     sig  = net->signalStrength();
        auto          ap   = net->referenceAccessPoint();
        const bool    sec  = ap && (ap->wpaFlags() || ap->rsnFlags());

        out.append(QVariantMap {
            { QStringLiteral("ssid"),          ssid },
            { QStringLiteral("active"),        ssid == activeSsid },
            { QStringLiteral("strength"),      sig },
            { QStringLiteral("strength_text"), QString::number(sig) + QLatin1Char('%') },
            { QStringLiteral("security"),      sec ? QStringLiteral("WPA") : QString() },
            { QStringLiteral("security_text"), sec ? QStringLiteral("Secured") : QStringLiteral("Open") },
            { QStringLiteral("saved"),         saved.contains(ssid) },
        });
    }
    return out;
}

int NmController::updateInterval() const { return m_updateInterval; }

void NmController::setUpdateInterval(int ms) {
    if (ms == m_updateInterval || ms < 100) return;
    m_updateInterval = ms;
    m_speedTimer.setInterval(ms);
    if (m_devStats) m_devStats->setRefreshRateMs(ms);
    emit updateIntervalChanged();
}

void NmController::toggleWifi()             { NetworkManager::setWirelessEnabled(!NetworkManager::isWirelessEnabled()); }
void NmController::enableWifi(bool enabled) { NetworkManager::setWirelessEnabled(enabled); }
void NmController::rescanWifi()             { if (auto wd = wirelessDevice()) wd->requestScan(); }

void NmController::connectToWifiNetwork(const QString& ssid, const QString& password) {
    auto wd = wirelessDevice();
    if (!wd) return;

    if (auto ap = wd->activeAccessPoint(); ap && ap->ssid() == ssid) return;

    const QString deviceUni = wd->uni();

    for (auto& c : NetworkManager::listConnections()) {
        auto s = c->settings();
        if (!s || s->connectionType() != NetworkManager::ConnectionSettings::Wireless) continue;
        auto ws = s->setting(NetworkManager::Setting::Wireless)
                      .staticCast<NetworkManager::WirelessSetting>();
        if (!ws || QString::fromUtf8(ws->ssid()) != ssid) continue;

        if (!password.isEmpty()) {
            auto sec = s->setting(NetworkManager::Setting::WirelessSecurity)
                           .staticCast<NetworkManager::WirelessSecuritySetting>();
            if (sec) {
                sec->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaPsk);
                sec->setPsk(password);
                sec->setInitialized(true);
            }
            const QString path = c->path();
            auto* w = new QDBusPendingCallWatcher(c->update(s->toMap()), this);
            connect(w, &QDBusPendingCallWatcher::finished, this,
                [w, path, deviceUni](QDBusPendingCallWatcher*) {
                    if (w->isError())
                        qWarning("NmController: update connection failed: %s",
                                 qPrintable(w->error().message()));
                    w->deleteLater();
                    NetworkManager::activateConnection(path, deviceUni, {});
                });
        } else {
            NetworkManager::activateConnection(c->path(), deviceUni, {});
        }
        return;
    }

    auto cs = QSharedPointer<NetworkManager::ConnectionSettings>::create(
        NetworkManager::ConnectionSettings::Wireless);
    cs->setId(ssid);
    cs->setAutoconnect(true);
    cs->setUuid(NetworkManager::ConnectionSettings::createNewUuid());

    auto ws = cs->setting(NetworkManager::Setting::Wireless)
                  .staticCast<NetworkManager::WirelessSetting>();
    ws->setSsid(ssid.toUtf8());
    ws->setMode(NetworkManager::WirelessSetting::Infrastructure);
    ws->setInitialized(true);

    auto ip4 = cs->setting(NetworkManager::Setting::Ipv4)
                   .staticCast<NetworkManager::Ipv4Setting>();
    ip4->setMethod(NetworkManager::Ipv4Setting::Automatic);
    ip4->setInitialized(true);

    auto ip6 = cs->setting(NetworkManager::Setting::Ipv6)
                   .staticCast<NetworkManager::Ipv6Setting>();
    ip6->setMethod(NetworkManager::Ipv6Setting::Automatic);
    ip6->setInitialized(true);

    if (!password.isEmpty()) {
        auto sec = cs->setting(NetworkManager::Setting::WirelessSecurity)
                       .staticCast<NetworkManager::WirelessSecuritySetting>();
        sec->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaPsk);
        sec->setPsk(password);
        sec->setInitialized(true);
    }

    auto* w = new QDBusPendingCallWatcher(
        NetworkManager::addAndActivateConnection(cs->toMap(), deviceUni, {}), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
        [w](QDBusPendingCallWatcher*) {
            if (w->isError())
                qWarning("NmController: addAndActivateConnection failed: %s",
                         qPrintable(w->error().message()));
            w->deleteLater();
        });
}

void NmController::disconnectWifiNetwork() {
    auto wd = wirelessDevice();
    if (!wd) return;
    const QString deviceUni = wd->uni();
    for (auto& ac : NetworkManager::activeConnections()) {
        if (ac && ac->devices().contains(deviceUni)) {
            NetworkManager::deactivateConnection(ac->path());
            return;
        }
    }
}

void NmController::forgetWifiNetwork(const QString& ssid) {
    for (auto& c : NetworkManager::listConnections()) {
        auto ws = wirelessSetting(c);
        if (ws && QString::fromUtf8(ws->ssid()) == ssid) {
            QDBusMessage msg = QDBusMessage::createMethodCall(
                QStringLiteral("org.freedesktop.NetworkManager"),
                c->path(),
                QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"),
                QStringLiteral("Delete"));
            QDBusConnection::systemBus().asyncCall(msg);
            break;
        }
    }
}
