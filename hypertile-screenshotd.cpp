#include <QApplication>
#include <QCommandLineParser>
#include <QSqlDatabase>
#include <QDebug>

#include "WebScreenshotter.h"

int main(int argc, char **argv)
{
    // 🔥 GPU KOMPLETT AUS – MUSS VOR QApplication!
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-gpu "
        "--disable-gpu-compositing "
        "--disable-software-rasterizer "
        "--disable-accelerated-2d-canvas "
        "--disable-accelerated-video-decode "
        "--disable-zero-copy "
        "--disable-features=VizDisplayCompositor"
    );

    QApplication app(argc, argv);
    app.setApplicationName("hypertile-screenshotd");

    // ------------------------------------------------
    // CLI
    // ------------------------------------------------
    QCommandLineParser parser;
    parser.setApplicationDescription("HyprTile Website Screenshot Daemon");
    parser.addHelpOption();

    QCommandLineOption dbOpt(
        {"d", "db"},
        "Path to monitor SQLite database",
        "file",
        "monitor.db"
    );

    parser.addOption(dbOpt);
    parser.process(app);

    QString dbPath = parser.value(dbOpt);

    // ------------------------------------------------
    // DB öffnen
    // ------------------------------------------------
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "❌ Cannot open DB:" << db.lastError().text();
        return 1;
    }

    // ------------------------------------------------
    // URLs laden
    // ------------------------------------------------
    QSqlQuery q(db);
    q.exec("SELECT url FROM monitor_urls");

    QVector<CaptureTask> tasks;
    while (q.next()) {
        tasks.append({
            q.value(0).toString(),
            1920, 1080,
            false,
            300, 200
        });
    }

    if (tasks.isEmpty()) {
        qDebug() << "No URLs to monitor.";
        return 0;
    }

    // ------------------------------------------------
    // Screenshotter starten
    // ------------------------------------------------
    auto *ws = new WebScreenshotter();

    ws->setDatabase(db);
    ws->setTasks(tasks);

    QObject::connect(ws, &WebScreenshotter::allDone, &app, [&]() {
        qDebug() << "✔ All screenshots done";
        app.quit();
    });

    ws->start();

    return app.exec();
}
