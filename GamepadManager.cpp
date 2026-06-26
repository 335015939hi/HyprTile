#include "GamepadManager.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <errno.h>
#include <QCryptographicHash>

/* ============================================================
 * Lifecycle
 * ============================================================ */

GamepadManager::GamepadManager(QObject* parent)
    : QObject(parent)
{
    connect(&scanTimer, &QTimer::timeout, this, &GamepadManager::scanDevices);
    scanTimer.start(1000);
    scanDevices();
}

GamepadManager::~GamepadManager() {
    closeDevice();
}

/* ============================================================
 * Device discovery
 * ============================================================ */

void GamepadManager::scanDevices() {
    if (fd >= 0) {
        if (!devPath.isEmpty() && !QFile::exists(devPath)) {
            qDebug() << "[Gamepad] device removed:" << devPath;
            closeDevice();
        }
        return;
    }
    openFirstAvailable();
}

bool GamepadManager::openFirstAvailable() {
    for (int i = 0; i < 16; ++i) {
        const QString path = QString("/dev/input/js%1").arg(i);
        if (!QFile::exists(path))
            continue;

        int nfd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (nfd < 0)
            continue;

        char name[128]{};
        if (ioctl(nfd, JSIOCGNAME(sizeof(name)), name) < 0) {
            ::close(nfd);
            continue;
        }

        devName = QString::fromUtf8(name);
        devGuid = linuxGuidFromJs(nfd);

        qDebug() << "[Gamepad] found:" << devName << "GUID:" << devGuid;

        bindings.clear();
        digitalState.clear();

        if (!devGuid.isEmpty() && loadMappingFromDB(devGuid)) {
            qDebug() << "[Gamepad] using gamecontrollerdb mapping";
        } else {
            qDebug() << "[Gamepad] no DB mapping → fallback";
            setupFallbackMapping();
        }

        fd = nfd;
        devPath = path;

        notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated,
                this, &GamepadManager::onReadable);

        qDebug() << "[Gamepad] opened" << devPath;
        return true;
    }
    return false;
}

void GamepadManager::closeDevice() {
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
        notifier = nullptr;
    }

    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }

    bindings.clear();
    digitalState.clear();

    devName.clear();
    devGuid.clear();
    devPath.clear();
}

/* ============================================================
 * GUID (SDL compatible)
 * ============================================================ */

QString GamepadManager::linuxGuidFromJs(int fd) {
    char name[128]{};
    if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0)
        return {};

    quint8 axes = 0;
    quint8 buttons = 0;

    ioctl(fd, JSIOCGAXES, &axes);
    ioctl(fd, JSIOCGBUTTONS, &buttons);

    // Stabile Pseudo-GUID (SDL-kompatibel genug für DB-Matches)
    QByteArray base = QByteArray(name)
                    + QByteArray::number(axes)
                    + QByteArray::number(buttons);

    QByteArray hash = QCryptographicHash::hash(
        base, QCryptographicHash::Md5
    ).toHex();

    // SDL-style GUID: 32 hex chars
    return QString::fromLatin1(hash.left(32));
}


/* ============================================================
 * gamecontrollerdb.txt (relative to binary)
 * ============================================================ */

bool GamepadManager::loadMappingFromDB(const QString& guid) {
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QString dbPath  = baseDir + "/misc/gamecontrollerdb.txt";

    QFile f(dbPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[Gamepad] cannot open gamecontrollerdb:"
                   << dbPath;
        return false;
    }

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith("#"))
            continue;

        const QStringList parts = line.split(",");
        if (parts.size() < 3)
            continue;

        if (parts[0] != guid && !devName.contains(parts[1], Qt::CaseInsensitive))
			continue;

        qDebug() << "[Gamepad] DB match:" << parts[1];

        for (int i = 2; i < parts.size(); ++i) {
            const QString p = parts[i];
            if (!p.contains(":"))
                continue;

            const auto kv = p.split(":");
            const QString name = kv[0];
            const QString val  = kv[1];

            InputBinding b;

            if (val.startsWith("b")) {
                b.type = InputBinding::Button;
                b.index = val.mid(1).toInt();
            } else if (val.startsWith("a")) {
                b.type = InputBinding::Axis;
                b.index = val.mid(1).toInt();
            } else if (val.startsWith("h")) {
                const auto hv = val.mid(1).split(".");
                if (hv.size() == 2) {
                    b.type = InputBinding::Hat;
                    b.index = hv[0].toInt();
                    b.hatMask = hv[1].toInt();
                }
            }

            bindings[name] = b;
        }
        return true;
    }
    return false;
}

/* ============================================================
 * Action mapping
 * ============================================================ */

QString GamepadManager::mapAction(const QString& dbName) const {
    static const QHash<QString, QString> map = {
        {"a", "confirm"},
        {"b", "back"},
        {"x", "alt"},
        {"y", "special"},
        {"dpup", "up"},
        {"dpdown", "down"},
        {"dpleft", "left"},
        {"dpright", "right"},
        {"start", "menu"},
        {"back", "back"}
    };

    return map.value(dbName, dbName);
}

/* ============================================================
 * Fallback Mapping
 * ============================================================ */

void GamepadManager::setupFallbackMapping() {
    bindings.clear();

    bindings["confirm"] = { InputBinding::Button, 0, 0 };
    bindings["back"]    = { InputBinding::Button, 1, 0 };
    bindings["menu"]    = { InputBinding::Button, 7, 0 };
    bindings["leftx"]   = { InputBinding::Axis,   0, 0 };
    bindings["lefty"]   = { InputBinding::Axis,   1, 0 };

    qDebug() << "[Gamepad] fallback mapping active";
}

/* ============================================================
 * Input processing
 * ============================================================ */

void GamepadManager::onReadable() {
    if (fd < 0)
        return;

    js_event e;
    while (true) {
        const ssize_t r = ::read(fd, &e, sizeof(e));

        if (r == 0) {
            qDebug() << "[Gamepad] EOF -> disconnected";
            closeDevice();
            QTimer::singleShot(0, this, &GamepadManager::scanDevices);
            return;
        }

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            qDebug() << "[Gamepad] read error errno=" << errno;
            closeDevice();
            QTimer::singleShot(0, this, &GamepadManager::scanDevices);
            return;
        }

        if (r != sizeof(e))
            break;

        if (e.type & JS_EVENT_INIT)
            continue;

        if (e.type & JS_EVENT_BUTTON)
            handleButton(e.number, e.value);
        else if (e.type & JS_EVENT_AXIS)
            handleAxis(e.number, e.value);
    }
}

void GamepadManager::handleButton(int idx, bool pressed) {
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        const InputBinding& b = it.value();
        if (b.type == InputBinding::Button && b.index == idx) {
            fireRepeatable(mapAction(it.key()), pressed);
        }
    }
}

void GamepadManager::handleAxis(int idx, int value) {

    /* =======================
     * AXIS (Sticks)
     * ======================= */
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        const InputBinding& b = it.value();

        if (b.type == InputBinding::Axis && b.index == idx) {
            int s = (value > deadzone) ? 1 : (value < -deadzone ? -1 : 0);
            const QString name = it.key();

            if (name.endsWith("x")) {
                updateDigitalFromAxis("right", s == 1);
                updateDigitalFromAxis("left",  s == -1);
            } else if (name.endsWith("y")) {
                updateDigitalFromAxis("down", s == 1);
                updateDigitalFromAxis("up",   s == -1);
            }
        }
    }

    /* =======================
     * HAT (D-Pad)
     * ======================= */
    // value ist Bitmaske (1,2,4,8)
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        const InputBinding& b = it.value();

        if (b.type != InputBinding::Hat || b.index != idx)
            continue;

        bool pressed = (value & b.hatMask) != 0;
        const QString action = mapAction(it.key());

        const bool old = digitalState.value(action, false);
        if (old == pressed)
            continue;

        digitalState[action] = pressed;
        fireRepeatable(action, pressed);
    }
}


void GamepadManager::updateDigitalFromAxis(const QString& actionName, bool pressed) {
    const bool old = digitalState.value(actionName, false);
    if (old == pressed)
        return;

    digitalState[actionName] = pressed;
    fireRepeatable(actionName, pressed);
}

void GamepadManager::fireRepeatable(const QString& actionName, bool pressed) {
    emit action(actionName, pressed);
}
