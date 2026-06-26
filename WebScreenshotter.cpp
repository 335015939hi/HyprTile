#include "WebScreenshotter.h"

#include <QTimer>
#include <QDebug>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineLoadingInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>

WebScreenshotter::WebScreenshotter(QObject *parent)
    : QObject(parent)
{
    // Profile erstellen
    profileDesktop = new QWebEngineProfile("desktop");
    profileMobile  = new QWebEngineProfile("mobile");

    // User Agents
    profileDesktop->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64; rv:131.0) Gecko/20100101 Firefox/131.0"
    );
    profileMobile->setHttpUserAgent(
        "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
        "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1"
    );

    // Headless View-Fenster
	view = new QWebEngineView();

	// Wichtig für Hyprland:
	view->setWindowTitle("WebScreenshotter");
	view->setObjectName("WebScreenshotter");

	// (optional aber sehr stark für Wayland)
	view->setProperty("app_id", "WebScreenshotter");

	// Flags: kein Fokus, keine Deko, kein Manager
	view->setWindowFlags(
		Qt::Tool |
		Qt::FramelessWindowHint |
		Qt::WindowDoesNotAcceptFocus |
		Qt::BypassWindowManagerHint   // <- Hyprland ignoriert es so gut wie komplett
	);

	view->move(-3000, -3000);
	view->resize(800, 600);
	view->show();
}

void WebScreenshotter::setDatabase(const QSqlDatabase &database)
{
    db = database;
}

void WebScreenshotter::setTasks(const QVector<CaptureTask> &newTasks)
{
    tasks = newTasks;
}

void WebScreenshotter::start()
{
    currentIndex = -1;
    runNextTask();
}

void WebScreenshotter::runNextTask()
{
    currentIndex++;

    if (currentIndex >= tasks.size()) {
        emit allDone();
        return;
    }

    const CaptureTask &t = tasks[currentIndex];

    // Alte Seite löschen
    if (view->page())
        view->page()->deleteLater();

    // Neue Page setzen
    auto *page = new QWebEnginePage(
        t.mobile ? profileMobile : profileDesktop
    );

    view->setPage(page);
    view->resize(t.width, t.height);

    //
    // === Gemeinsame Variable für HTTP-Status ===
    //
    auto httpStatusPtr = QSharedPointer<int>::create(0);

    //
    // === HTTP STATUS per loadingChanged erfassen ===
    //
    connect(page, &QWebEnginePage::loadingChanged, this,
        [=](const QWebEngineLoadingInfo &info)
        {
            // Nur das Hauptdokument
            if (info.url() != page->url())
                return;

            // Nur finaler Zustand
            if (info.status() != QWebEngineLoadingInfo::LoadSucceededStatus &&
                info.status() != QWebEngineLoadingInfo::LoadFailedStatus)
                return;

            // Nur HTTP Fehler/Erfolg
            if (info.errorDomain() != QWebEngineLoadingInfo::HttpStatusCodeDomain)
                return;

            // HTTP-Code setzen
            *(httpStatusPtr.data()) = info.errorCode();

            // qDebug() << "[HTTP] Final Status =" << *(httpStatusPtr.data());
        }
    );

    //
    // ===== Callback nach dem Laden =====
    //
    connect(page, &QWebEnginePage::loadFinished, this,
        [=](bool ok)
        {
            //
            // HTTP-Status jetzt aus httpStatusPtr lesen:
            //
            int httpStatus = *httpStatusPtr.data();

            if (!ok) {
                // → Fehlerhafte Seite, trotzdem HTTP-Status speichern (falls vorhanden)
                QSqlQuery ins(db);
                ins.prepare(
                    "INSERT INTO monitor_events "
                    "(url, ts, http_code, ok, screenshot_base64, thumb_base64) "
                    "VALUES (?, ?, ?, ?, ?, ?)"
                );

                ins.addBindValue(t.url);
                ins.addBindValue(QDateTime::currentSecsSinceEpoch());
                ins.addBindValue(httpStatus);
                ins.addBindValue(0);
                ins.addBindValue("");
                ins.addBindValue("");
                ins.exec();

                emit screenshotFailed(t.url, "Page failed to load");
                runNextTask();
                return;
            }

            //
            // Performance-Daten aus JS
            //
            page->runJavaScript(
                "var p = performance.getEntriesByType('navigation')[0];"
                "p ? { "
                " dns: p.domainLookupEnd - p.domainLookupStart, "
                " connect: p.connectEnd - p.connectStart, "
                " ttfb: p.responseStart - p.requestStart, "
                " domready: p.domContentLoadedEventEnd, "
                " onload: p.loadEventEnd, "
                " total: p.duration "
                "} : null;",
                [=](const QVariant &perf)
                {
                    QTimer::singleShot(300, this, [=]()
                    {
                        QImage img = view->grab().toImage();

                        // Screenshot → WEBP Base64
                        QByteArray ba;
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "WEBP", 100);
                        QString base64Screenshot = ba.toBase64();

                        // Thumbnail
                        QString base64Thumb;
                        if (t.thumbWidth > 0 && t.thumbHeight > 0) {
                            QImage thumb = img.scaled(
                                t.thumbWidth,
                                t.thumbHeight,
                                Qt::KeepAspectRatio,
                                Qt::SmoothTransformation
                            );

                            QByteArray tb;
                            QBuffer tbuf(&tb);
                            tbuf.open(QIODevice::WriteOnly);
                            thumb.save(&tbuf, "WEBP", 100);
                            base64Thumb = tb.toBase64();
                        }

                        // Performance
                        QVariantMap pm = perf.toMap();

                        int dns      = pm.value("dns").toInt();
                        int connect  = pm.value("connect").toInt();
                        int ttfb     = pm.value("ttfb").toInt();
                        int domready = pm.value("domready").toInt();
                        int onload   = pm.value("onload").toInt();
                        int total    = pm.value("total").toInt();

                        //
                        // === DB INSERT ===
                        //
                        QSqlQuery ins(db);
                        ins.prepare(
                            "INSERT INTO monitor_events "
                            "(url, ts, http_code, dns_ms, connect_ms, ttfb_ms, "
                            " domready_ms, onload_ms, total_ms, ok, "
                            " screenshot_base64, thumb_base64) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                        );

                        int httpStatus = *httpStatusPtr.data();  // <- hier wird er gespeichert

                        ins.addBindValue(t.url);
                        ins.addBindValue(QDateTime::currentSecsSinceEpoch());
                        ins.addBindValue(httpStatus);
                        ins.addBindValue(dns);
                        ins.addBindValue(connect);
                        ins.addBindValue(ttfb);
                        ins.addBindValue(domready);
                        ins.addBindValue(onload);
                        ins.addBindValue(total);
                        ins.addBindValue(1);
                        ins.addBindValue(base64Screenshot);
                        ins.addBindValue(base64Thumb);

                        if (!ins.exec())
                            qDebug() << "DB ERROR:" << ins.lastError().text();

                        page->deleteLater();
                        runNextTask();
                    });
                }
            );
        }
    );

    page->load(QUrl(t.url));
}

