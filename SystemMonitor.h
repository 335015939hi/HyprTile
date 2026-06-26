#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QTimer>
#include <QFile>

class SystemMonitor : public QObject {
    Q_OBJECT

public:
    explicit SystemMonitor(QObject* parent = nullptr);

signals:
    void sysInfo(double cpu, double ramUsed, double ramTotal, double cpuTemp);

private:
    QTimer* timer;

    long lastUser = 0;
    long lastNice = 0;
    long lastSys  = 0;
    long lastIdle = 0;

    bool firstCpuRead = true;   // optional

    double getCpuUsage();
    double getRamTotal();
    double getRamUsed();
    double getCpuTemp();        // neu
    double getCpuTempHwmon();        // neu
};

#endif
