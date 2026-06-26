#include "SystemMonitor.h"
#include <QDebug>
#include <QRegularExpression>
#include <QDir>

static QString readTextFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(f.readAll()).trimmed();
}

static long readLongFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return 0;
    bool ok = false;
    long v = QString::fromLatin1(f.readAll()).trimmed().toLong(&ok);
    return ok ? v : 0;
}

static long meminfoValue(const char* key)
{
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly)) {
        //qDebug() << "❌ meminfo nicht lesbar!";
        return 0;
    }

    QByteArray data = f.readAll();
    QString text = QString::fromLatin1(data);

    // ^Key:\s*(\d+)  multiline
    QRegularExpression re(
        QString("^%1:\\s*(\\d+)").arg(key),
        QRegularExpression::MultilineOption);

    auto match = re.match(text);
    if (!match.hasMatch())
        return 0;

    return match.captured(1).toLong();
}


SystemMonitor::SystemMonitor(QObject* parent)
    : QObject(parent)
{
   // qDebug() << "✅ SystemMonitor.cpp BUILD MARKER:"
     //        << __FILE__ << __DATE__ << __TIME__;

    timer = new QTimer(this);

    connect(timer, &QTimer::timeout, this, [this]() {
        double cpu  = getCpuUsage();
        double ramT = getRamTotal();
        double ramU = getRamUsed();
        double temp = getCpuTemp();

        emit sysInfo(cpu, ramU, ramT, temp);
    });

    timer->start(1000); // 1 Sekunde
}

double SystemMonitor::getCpuUsage() {
    QFile f("/proc/stat");
    if (!f.open(QFile::ReadOnly)) {
        //qDebug() << "❌ CPU: /proc/stat nicht lesbar!";
        return -1;
    }

    QByteArray line = f.readLine();
    QList<QByteArray> parts = line.split(' ');

    if (parts.size() < 8) {
        //qDebug() << "❌ CPU: ungültige /proc/stat Zeile:" << line;
        return -1;
    }

    long user = parts[2].toLong();
    long nice = parts[3].toLong();
    long sys  = parts[4].toLong();
    long idle = parts[5].toLong();

    long dUser = user - lastUser;
    long dNice = nice - lastNice;
    long dSys  = sys  - lastSys;
    long dIdle = idle - lastIdle;

    lastUser = user;
    lastNice = nice;
    lastSys  = sys;
    lastIdle = idle;

    long total = dUser + dNice + dSys + dIdle;

    if (total == 0) return 0;

    double usage = (double)(dUser + dNice + dSys) / total * 100.0;

    return usage;
}

double SystemMonitor::getRamTotal()
{
    long memTotal = meminfoValue("MemTotal");
    if (memTotal <= 0) {
        //qDebug() << "❌ RAM Total konnte nicht ermittelt werden!";
        return -1;
    }
    return memTotal / 1024.0;  // MiB
}



double SystemMonitor::getRamUsed()
{
    long memTotal = meminfoValue("MemTotal");
    if (memTotal <= 0) {
        //qDebug() << "❌ RAM Used: MemTotal fehlt!";
        return -1;
    }

    long memAvail = meminfoValue("MemAvailable");

    // Fallback falls MemAvailable nicht existiert
    if (memAvail <= 0) {
        long memFree  = meminfoValue("MemFree");
        long buffers  = meminfoValue("Buffers");
        long cached   = meminfoValue("Cached");
        long sreclaim = meminfoValue("SReclaimable");
        long shmem    = meminfoValue("Shmem");

        memAvail = memFree + buffers + cached + sreclaim - shmem;
        if (memAvail < 0) memAvail = 0;

        //qDebug() << "ℹ️ MemAvailable fehlte -> Fallback genutzt avail(kB)=" << memAvail;
    }

    long used = memTotal - memAvail;
    if (used < 0) used = 0;

    return used / 1024.0;  // MiB
}

double SystemMonitor::getCpuTemp()
{
    QDir thermalDir("/sys/class/thermal");
    const auto zones = thermalDir.entryList({"thermal_zone*"},
                                            QDir::Dirs | QDir::NoDotAndDotDot);

    auto readZoneTemp = [&](const QString& wantedType) -> double {
        for (const auto& z : zones) {
            QString base = thermalDir.absoluteFilePath(z);
            QString type = readTextFile(base + "/type");  // helper von vorher
            if (type == wantedType) {
                long milli = readLongFile(base + "/temp"); // helper von vorher
                if (milli > 0) return milli / 1000.0;
            }
        }
        return -1;
    };

    // 1. Laptopsensoren (falls vorhanden)
    double t = readZoneTemp("x86_pkg_temp");
    if (t >= 0) return t;

    t = readZoneTemp("TCPU");
    if (t >= 0) return t;

    // 2. Desktop/AMD/Intel PC mit hwmon
    t = getCpuTempHwmon();
    if (t >= 0) return t;

    return -1;
}


double SystemMonitor::getCpuTempHwmon()
{
    QDir hwmonDir("/sys/class/hwmon");
    const auto dirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto& entry : dirs) {
        QString base = hwmonDir.absoluteFilePath(entry);
        QString name = readTextFile(base + "/name").trimmed();

        // Wir suchen speziell deine AMD-CPU
        if (name == "k10temp") {

            // temp1_input = CPU Tctl/Tdie
            QString cpuTempFile = base + "/temp1_input";

            long milli = readLongFile(cpuTempFile);
            if (milli > 0)
                return milli / 1000.0;
        }
    }

    // Falls jemand eine Intel/anderes System nutzt:
    return -1;
}


