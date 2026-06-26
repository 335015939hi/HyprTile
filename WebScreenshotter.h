#pragma once

#include <QObject>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
#include <QImage>
#include <QBuffer>
#include <QIODevice>

// ---- Task-Struktur ----
struct CaptureTask {
    QString url;
    int width;
    int height;
    bool mobile;

    // Optional Thumbnail
    int thumbWidth;
    int thumbHeight;
};


// ---- WebScreenshotter-Klasse ----
class WebScreenshotter : public QObject {
    Q_OBJECT

public:
    explicit WebScreenshotter(QObject *parent = nullptr);

    // Datenbank setzen
    void setDatabase(const QSqlDatabase &database);

    // Liste der Screenshot-Aufgaben setzen
    void setTasks(const QVector<CaptureTask> &newTasks);

    // Verarbeitung starten
    void start();


signals:
    void allDone();
    void screenshotFailed(const QString &url, const QString &reason);

private:
    void runNextTask();

    QWebEngineView *view;

    QWebEngineProfile *profileDesktop;
    QWebEngineProfile *profileMobile;

    QVector<CaptureTask> tasks;
    int currentIndex = -1;

    QSqlDatabase db;
};

