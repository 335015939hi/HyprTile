// -------------------- IMPORTS --------------------
#include "index.html.h"
#include <QApplication>
#include <QWebEngineView>
#include <QWebChannel>
#include <QKeyEvent>
#include <QProcess>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWindow>
#include <QFile>
#include <QJsonArray>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QThread>
#include <QThreadPool>
#include <QFileSystemWatcher>
#include <QtConcurrent>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>

#include <QDirIterator>
#include <QTextStream>
#include "SystemMonitor.h"

#include <QCoreApplication>
#include "PluginManager.h"

#include <signal.h>
#include <cstdlib>
#include <QByteArray>

#include <QFileDialog>

#include <GamepadManager.h>

static int stderrPipeFd[2];

static void installWebEngineStderrFilter()
{
    if (pipe(stderrPipeFd) != 0)
        return;

    // STDERR auf Pipe umleiten
    dup2(stderrPipeFd[1], STDERR_FILENO);
    close(stderrPipeFd[1]);

    // Qt Eventloop liest STDERR
    auto *notifier = new QSocketNotifier(
        stderrPipeFd[0],
        QSocketNotifier::Read,
        QCoreApplication::instance()
    );

    QObject::connect(
        notifier,
        &QSocketNotifier::activated,
        [](int fd)
        {
            char buf[4096];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0)
                return;

            QByteArray msg(buf, n);

            // 🔕 GENAU diese Meldungen filtern
            if (msg.contains("Error with Permissions-Policy header"))
                return;

            // alles andere normal ausgeben
            write(STDERR_FILENO, msg.constData(), msg.size());
        }
    );
}

void ensureMonitorSchema(QSqlDatabase &db)
{
    QSqlQuery q(db);

    q.exec(
        "CREATE TABLE IF NOT EXISTS monitor_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "url TEXT NOT NULL,"
        "ts INTEGER NOT NULL,"
        "http_code INTEGER,"
        "dns_ms INTEGER,"
        "connect_ms INTEGER,"
        "ttfb_ms INTEGER,"
        "domready_ms INTEGER,"
        "onload_ms INTEGER,"
        "total_ms INTEGER,"
        "ok INTEGER,"
        "screenshot_base64 TEXT,"
        "thumb_base64 TEXT)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS monitor_urls ("
        "url TEXT PRIMARY KEY)"
    );

    q.exec("CREATE INDEX IF NOT EXISTS idx_monitor_ts ON monitor_events(ts)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_monitor_url_ts ON monitor_events(url, ts)");

    q.exec(
        "CREATE TABLE IF NOT EXISTS monitor_agg ("
        "url TEXT,"
        "bucket INTEGER,"
        "start_ts INTEGER,"
        "end_ts INTEGER,"
        "avg_total REAL,"
        "avg_ttfb REAL,"
        "avg_domready REAL,"
        "avg_onload REAL,"
        "success_rate REAL,"
        "checks INTEGER)"
    );

    q.exec("CREATE INDEX IF NOT EXISTS idx_agg ON monitor_agg(url, bucket, end_ts)");
}

void initNotesSchema(QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT,
            content TEXT,
            created_at TEXT,
            updated_at TEXT
        );
    )");
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS config (
			key TEXT PRIMARY KEY,
			value TEXT
		);
    )");
    q.exec(R"(
		CREATE TABLE IF NOT EXISTS events (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			date TEXT NOT NULL,          -- Format: YYYY-MM-DD
			title TEXT NOT NULL,
			allday INTEGER NOT NULL,     -- 1 = ganztägig, 0 = Zeitbereich
			start_time TEXT,             -- NULL wenn allday=1
			end_time TEXT,
			repeat_yearly INTEGER NOT NULL DEFAULT 0
		);
	)");

}




static const char* SOCKET_PATH = "/tmp/launcher.sock";

class CustomPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit CustomPage(QObject *parent = nullptr) : QWebEnginePage(parent) {}

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override
    {
        if (type == QWebEnginePage::NavigationTypeLinkClicked) {

            // Extern öffnen
            QProcess::startDetached("xdg-open", { url.toString() });

            return false; // Laden im WebView verhindern
        }

        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};
/* ===========================================================
  BACKEND
=========================================================== */

class Backend : public QObject {
    Q_OBJECT

public:
     QJsonObject config;
    SystemMonitor* sysmon = nullptr;

    QSqlDatabase monitorDB;
    QSqlDatabase notesDB;
	
	QString currentRecBase;
	bool audioRecordingActive = false;
	
    Backend(QObject* parent=nullptr)
        : QObject(parent)
    {
        // ----------------- SYSTEM MONITOR -----------------
        sysmon = new SystemMonitor(this);
        connect(sysmon, &SystemMonitor::sysInfo,
                this, &Backend::sendSysInfo3);

        // ----------------- NOTES DB -----------------
        notesDB = QSqlDatabase::addDatabase("QSQLITE", "notes");
        notesDB.setDatabaseName("notes.db");
		
        if (!notesDB.open())
            qWarning() << "❌ notes.db konnte nicht geöffnet werden:" << notesDB.lastError();
        else
            initNotesSchema(notesDB);

        // ----------------- MONITOR DB -----------------
        monitorDB = QSqlDatabase::addDatabase("QSQLITE", "monitor");
        monitorDB.setDatabaseName("monitor.db");

        if (!monitorDB.open())
            qWarning() << "❌ monitor.db konnte nicht geöffnet werden:" << monitorDB.lastError();
        else
            ensureMonitorSchema(monitorDB);

        // ----------------- 5-MINUTE TIMER (aligned to wall clock) -----------------
		QTimer::singleShot(0, this, [this]() {

			QTime now = QTime::currentTime();
			int minute = now.minute();
			int second = now.second();
			int msec   = now.msec();

			// Zielminuten: 0,5,10,15,...
			int next5 = ((minute / 5) + 1) * 5;
			if (next5 >= 60) next5 = 0;  // wrap hour

			// Zeit bis zur nächsten 5-Minuten-Marke:
			int msToNext = 
				now.msecsTo(QTime(now.hour(), next5, 0, 0));
			if (msToNext < 0)
				msToNext += 60 * 60 * 1000; // Stunde weiter

			//qDebug() << "⏳ Monitor-Sync in" << msToNext << "ms";

			// 1) Einmalige Ausrichtung
			QTimer::singleShot(msToNext, this, [this]() {

				//qDebug() << "🟢 Monitor run ALIGN at full 5-minute mark";
				runMonitorScan();

				// 2) Danach: echter 5-Minuten-Timer
				QTimer* t = new QTimer(this);
				t->setInterval(5 * 60 * 1000);
				connect(t, &QTimer::timeout, this, &Backend::runMonitorScan);
				t->start();

				//qDebug() << "🔁 Monitor running every 5 minutes at 0/5/10...";
			});
		});
			
		// -------- MUSIC ----------
		QString musicBlob = getConfigValue("music_index", "");
		if (!musicBlob.isEmpty()) {
			allMusic = musicBlob.split('\n', Qt::SkipEmptyParts);
		}

		// -------- VIDEO ----------
		QString videoBlob = getConfigValue("video_index", "");
		if (!videoBlob.isEmpty()) {
			allVideos = videoBlob.split('\n', Qt::SkipEmptyParts);
		}
		
		QTimer::singleShot(0, this, [this]() {
			scheduleTodayEventTimers();
		});
		
		
    }

public slots:

    /* --------------------- APP START --------------------- */
    void launch(const QString& id) {
        if (!config.contains("apps")) return;

        QJsonArray arr = config["apps"].toArray();

        QString cmdLine;
        for (auto item : arr) {
            QJsonObject o = item.toObject();
            if (o["id"].toString() == id)
                cmdLine = o["cmd"].toString();
        }

        if (cmdLine.isEmpty())
            return;

        QStringList parts = cmdLine.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) return;

        QString program = parts.takeFirst();
        QStringList args = parts;

        QProcess::startDetached(program, args);

        emit requestToggle();
    }
    
    void sendSysInfo3(double cpu, double ramUsed, double ramTotal, double cpuTemp) {
		emit sendSysInfo(cpu, ramUsed, ramTotal, cpuTemp, 0);
	}
	
	void frontendReady() {
		frontendIsReady = true;
		emit frontendReadySignal();
	}



    /* --------------------- HYPRLAND EXIT --------------------- */
    void exitHyprland() {
        QProcess::startDetached("hyprctl", {"dispatch", "exit"});
    }

    void toggleFromJS() {
        emit requestToggle();
    }
    
    Q_INVOKABLE QString screenshotPath() const;
    Q_INVOKABLE QString recordPath() const;

    /* --------------------- VOLUME / PACTL --------------------- */
    QString getActiveSink() {
        QProcess p;
        p.start("pactl", {"list", "sinks"});
        p.waitForFinished();

        QString out = p.readAllStandardOutput();
        QStringList blocks = out.split("Sink #");

        for (const QString &block : blocks) {
            if (block.contains("State: RUNNING")) {
                QRegularExpression re("Name:\\s*(\\S+)");
                auto m = re.match(block);
                if (m.hasMatch())
                    return m.captured(1).trimmed();
            }
        }
        return "";
    }

    QString getBestSink() {
        QString active = getActiveSink();
        if (!active.isEmpty())
            return active;

        QProcess p;
        p.start("pactl", {"get-default-sink"});
        p.waitForFinished();

        QString def = p.readAllStandardOutput().trimmed();
        if (!def.isEmpty())
            return def;

        return "@DEFAULT_SINK@";
    }

    void volumeUp() {
        QString sink = getBestSink();
        QProcess::startDetached("pactl", {"set-sink-volume", sink, "+5%"});
        sendVolumeToFrontend();
    }

    void volumeDown() {
        QString sink = getBestSink();
        QProcess::startDetached("pactl", {"set-sink-volume", sink, "-5%"});
        sendVolumeToFrontend();
    }

    void volumeUpSmall() {
        QString sink = getBestSink();
        QProcess::startDetached("pactl", {"set-sink-volume", sink, "+3%"});
        sendVolumeToFrontend();
    }

    void volumeDownSmall() {
        QString sink = getBestSink();
        QProcess::startDetached("pactl", {"set-sink-volume", sink, "-3%"});
        sendVolumeToFrontend();
    }

    void volumeToggleMute() {
        QString sink = getBestSink();
        QProcess::startDetached("pactl", {"set-sink-mute", sink, "toggle"});
        sendVolumeToFrontend();
    }

    void sendVolumeToFrontend() {
        QString sink = getBestSink();

        QProcess p;
        p.start("pactl", {"get-sink-volume", sink});
        p.waitForFinished();

        QString out = p.readAllStandardOutput();
        QRegularExpression re("(\\d+)%");
        auto m = re.match(out);

        if (m.hasMatch()) {
            int pct = m.captured(1).toInt();
            if (pct > 100) {
                QProcess::startDetached("pactl", {"set-sink-volume", sink, "100%"});
                pct = 100;
            }
            emit sendVolume(pct / 100.0);
            return;
        }
        emit sendVolume(0.0);
    }
    
    Q_INVOKABLE void setVolume(int percent) {
		if (percent < 0) percent = 0;
		if (percent > 100) percent = 100;

		QString sink = getBestSink();
		QProcess::startDetached("pactl", {"set-sink-volume", sink, QString::number(percent) + "%"});
		sendVolumeToFrontend();
	}


    /* --------------------- SORTIERUNG --------------------- */
    Q_INVOKABLE void saveReorderedApps(const QStringList& ids) {
		if (!config.contains("apps")) return;

		QJsonArray oldApps = config["apps"].toArray();
		QJsonArray newApps;

		// Lookup nach ID
		QHash<QString, QJsonObject> lookup;
		for (const QJsonValue& v : oldApps) {
			QJsonObject o = v.toObject();
			lookup[o["id"].toString()] = o;
		}

		// *** NUR Apps hinzufügen, die noch in ids sind ***
		for (const QString& id : ids) {
			if (lookup.contains(id))
				newApps.append(lookup[id]);
		}

		// *** WICHTIG: NICHT zusätzlich die alten wieder anhängen! ***
		// → Dadurch wird eine App tatsächlich gelöscht.

		config["apps"] = newApps;

		QFile f("apps.json");
		if (f.open(QFile::WriteOnly)) {
			QJsonDocument doc(config);
			f.write(doc.toJson(QJsonDocument::Indented));
			f.close();
		}

		emit sendApps(newApps);
	}
	
	Q_INVOKABLE void saveFullApps(const QJsonArray &apps) {
		config["apps"] = apps;

		QFile f("apps.json");
		if (f.open(QFile::WriteOnly)) {
			QJsonDocument doc(config);
			f.write(doc.toJson(QJsonDocument::Indented));
			f.close();
		}

		emit sendApps(apps);
	}

	
	Q_INVOKABLE bool saveFile(const QString &path, const QString &base64) {
		QByteArray data = QByteArray::fromBase64(base64.toUtf8());
		QFile f(path);

		QDir().mkpath(QFileInfo(path).dir().path());

		if (!f.open(QFile::WriteOnly))
			return false;

		f.write(data);
		f.close();
		return true;
	}
	
	Q_INVOKABLE QStringList listIconsLimited(int limit = 50) {
		QStringList out;

		QStringList filters;
		filters << "*.svg" << "*.png" << "*.jpg" << "*.jpeg" << "*.webp";

		QDirIterator it("icons",
						filters,
						QDir::Files,
						QDirIterator::Subdirectories);

		int count = 0;
		while (it.hasNext() && count < limit) {
			out << it.next();
			count++;
		}

		return out;
	}

	QString getBestAudioSource() {
		QProcess p;
		p.start("pactl", {"list", "sources", "short"});
		p.waitForFinished();

		QString out = p.readAllStandardOutput();
		for (const QString &line : out.split('\n')) {
			if (line.contains(".monitor"))
				return line.split('\t')[1];
		}
		return "@DEFAULT_AUDIO_SOURCE@";
	}

    /* --------------------- SCREENSHOT / RECORD --------------------- */
    Q_INVOKABLE void capture(QString action, int delay, bool withAudio)
	{
		QString base = QCoreApplication::applicationDirPath();
		QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");

		// ------------------ SCREENSHOTS ------------------
		if (action == "shot-area" || action == "shot-screen") {

			QString cmd;

			if (action == "shot-area") {
				cmd = QString(
					"grim -g \"$(slurp)\" \"%1/Screenshots/shot-%2.png\""
				).arg(base, ts);
			} else {
				cmd = QString(
					"grim \"%1/Screenshots/screen-%2.png\""
				).arg(base, ts);
			}

			auto doShot = [cmd]() {
				QThreadPool::globalInstance()->start([cmd]() {
					system(cmd.toUtf8().constData());
				});
			};

			if (delay > 0) {
				QTimer::singleShot(delay * 1000, this, doShot);
			} else {
				doShot();
			}

			return;
		}

		// ------------------ RECORDING ------------------
		if (action != "rec-area" && action != "rec-screen")
			return;

		currentRecBase = base + "/ScreenRecords/rec-" + ts;

		QString videoCmd;
		if (action == "rec-area") {
			videoCmd =
				"wf-recorder -g \"$(slurp)\" -f \"" +
				currentRecBase + ".video.mp4\"";
		} else {
			videoCmd =
				"wf-recorder -f \"" +
				currentRecBase + ".video.mp4\"";
		}

		auto startRecording = [=]() {

			// 🎥 Video
			QThreadPool::globalInstance()->start([videoCmd]() {
				system(videoCmd.toUtf8().constData());
			});

			// 🎧 Audio (PULSE → STABIL!)
			if (withAudio) {

				QString audioCmd =
					"parecord "
					"--device=\"$(pactl get-default-sink).monitor\" "
					"--channels=2 "
					"--rate=48000 "
					"\"" + currentRecBase + ".audio.wav\"";

				audioRecordingActive = true;

				QThreadPool::globalInstance()->start([audioCmd]() {
					system(audioCmd.toUtf8().constData());
				});
			}

			emit recordingStarted();
		};

		// ------------------ DELAY ------------------
		if (delay > 0) {
			QTimer::singleShot(delay * 1000, this, startRecording);
		} else {
			startRecording();
		}
	}




    Q_INVOKABLE void stopRecording()
	{
		// 🎥 Video stoppen
		system("pkill -INT wf-recorder");

		// 🎧 Audio stoppen
		if (audioRecordingActive) {
			system("pkill -INT parecord");
			audioRecordingActive = false;

			QString outFile = currentRecBase + ".mp4";

			// 🔗 Mux (verlustfrei)
			QString muxCmd =
				"ffmpeg -y "
				"-i \"" + currentRecBase + ".video.mp4\" "
				"-i \"" + currentRecBase + ".audio.wav\" "
				"-c copy \"" + outFile + "\"";

			system(muxCmd.toUtf8().constData());

			// 🧹 Cleanup
			QFile::remove(currentRecBase + ".video.mp4");
			QFile::remove(currentRecBase + ".audio.wav");
		}
		else {
			// 🟢 Video-only
			QFile::rename(
				currentRecBase + ".video.mp4",
				currentRecBase + ".mp4"
			);
		}

		emit recordingStopped();
	}

    
    Q_INVOKABLE void openFolder(const QString &path) {

		QString fm = getConfigValue("file_manager", "");

		// 1️⃣ explizit gesetzter Dateimanager
		if (!fm.isEmpty()) {
			QStringList parts = fm.split(' ', Qt::SkipEmptyParts);
			QString program = parts.takeFirst();
			QStringList args = parts;
			args << path;
			QProcess::startDetached(program, args);
			return;
		}

		// 2️⃣ Fallback: xdg-open (Desktop-Default)
		QProcess::startDetached("xdg-open", { path });
	}


	
	Q_INVOKABLE QJsonArray loadNotes() {
        QJsonArray arr;
        QSqlQuery q(notesDB);
        q.exec("SELECT id, title, updated_at FROM notes ORDER BY updated_at DESC");

        while (q.next()) {
            QJsonObject o;
            o["id"] = q.value(0).toInt();
            o["title"] = q.value(1).toString();
            o["updated_at"] = q.value(2).toString();
            arr.append(o);
        }
        return arr;
    }
	
	Q_INVOKABLE QJsonObject loadNote(int id) {
        QSqlQuery q(notesDB);
        q.prepare("SELECT * FROM notes WHERE id=?");
        q.addBindValue(id);
        q.exec();

        if (q.next()) {
            QJsonObject o;
            o["id"] = id;
            o["title"] = q.value("title").toString();
            o["content"] = q.value("content").toString();
            return o;
        }
        return {};
    }
	
	Q_INVOKABLE int createNote() {
        QSqlQuery q(notesDB);
        q.exec("INSERT INTO notes (title, content, created_at, updated_at) "
               "VALUES ('','',datetime('now'),datetime('now'))");
        return q.lastInsertId().toInt();
    }

	Q_INVOKABLE bool saveNote(int id, QString title, QString content) {
        QSqlQuery q(notesDB);
        q.prepare("UPDATE notes SET title=?, content=?, updated_at=datetime('now') WHERE id=?");
        q.addBindValue(title);
        q.addBindValue(content);
        q.addBindValue(id);
        return q.exec();
    }

	Q_INVOKABLE bool deleteNote(int id) {
        QSqlQuery q(notesDB);
        q.prepare("DELETE FROM notes WHERE id=?");
        q.addBindValue(id);
        return q.exec();
    }
	
	Q_INVOKABLE void openExternalUrl(const QString &url) {
		QProcess::startDetached("firefox", { url });
	}
	
	Q_INVOKABLE QString getConfigValue(const QString &key, const QString &defaultVal) {
		QSqlQuery q(notesDB);
		q.prepare("SELECT value FROM config WHERE key=?");
		q.addBindValue(key);
		q.exec();
		if (q.next()) return q.value(0).toString();
		return defaultVal;
	}

	Q_INVOKABLE bool setConfigValue(const QString &key, const QString &value) {
		QSqlQuery q(notesDB);
		q.prepare("INSERT INTO config(key,value) VALUES(?,?) "
				  "ON CONFLICT(key) DO UPDATE SET value=excluded.value");
		q.addBindValue(key);
		q.addBindValue(value);
		return q.exec();
	}
    
    void runMonitorScan()
	{
		// Screenshot-Daemon starten (GPU-frei, eigener Prozess)
		QProcess::startDetached(
			QDir::currentPath() + "/hypertile-screenshotd",
			{ "--db=monitor.db" }
		);
	}

    
    Q_INVOKABLE bool addMonitorUrl(const QString &url)
	{
		QSqlQuery q(monitorDB);
		q.prepare("INSERT OR IGNORE INTO monitor_urls(url) VALUES(?)");
		q.addBindValue(url);
		return q.exec();
	}
    
    Q_INVOKABLE bool deleteMonitorUrl(const QString &url)
	{
		QSqlQuery q1(monitorDB);
		q1.prepare("DELETE FROM monitor_urls WHERE url = ?");
		q1.addBindValue(url);
		if (!q1.exec()) return false;

		QSqlQuery q2(monitorDB);
		q2.prepare("DELETE FROM monitor_events WHERE url = ?");
		q2.addBindValue(url);
		q2.exec();

		return true;
	}
	
	Q_INVOKABLE QJsonArray listMonitorUrls()
	{
		QJsonArray arr;

		QSqlQuery q(monitorDB);
		q.prepare("SELECT url FROM monitor_urls ORDER BY url ASC");
		q.exec();

		while (q.next()) {
			QJsonObject o;
			o["url"] = q.value(0).toString();
			arr.append(o);
		}
		return arr;
	}

	
	
	Q_INVOKABLE QJsonArray loadLatestMonitorEvents() {
		QJsonArray arr;

		QSqlQuery q(monitorDB);
		q.exec(R"(
			SELECT me.url,
				   me.http_code,
				   me.ts,
				   me.total_ms,
				   me.ttfb_ms,
				   me.domready_ms,
				   me.thumb_base64
			FROM monitor_events me
			INNER JOIN (
				SELECT url, MAX(ts) AS max_ts
				FROM monitor_events
				GROUP BY url
			) x ON me.url = x.url AND me.ts = x.max_ts
			ORDER BY me.url
		)");

		while (q.next()) {
			QString url = q.value(0).toString();

			QJsonObject o;
			o["url"]      = url;
			o["http"]     = q.value(1).toInt();
			o["ts"]       = q.value(2).toInt();
			o["total"]    = q.value(3).toInt();
			o["ttfb"]     = q.value(4).toInt();
			o["domready"] = q.value(5).toInt();
			o["thumb"]    = q.value(6).toString();

			// ---------------------------------------------
			// History: letzte 12 total_ms-Werte pro URL
			// ---------------------------------------------
			QJsonArray hist;

			QSqlQuery hq(monitorDB);
			hq.prepare("SELECT ttfb_ms FROM monitor_events WHERE url=? ORDER BY ts DESC LIMIT 12");
			hq.addBindValue(url);
			hq.exec();

			while (hq.next()) {
				hist.append(hq.value(0).toInt());
			}

			o["history"] = hist;

			arr.append(o);
		}

		return arr;
	}
	
	Q_INVOKABLE void scanMusicDir() {

		QString root = getConfigValue("music_root", QDir::homePath() + "/Musik");
		allMusic.clear();

		QDirIterator it(
			root,
			QStringList() << "*.mp3" << "*.MP3" << "*.flac" << "*.FLAC",
			QDir::Files,
			QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
		);

		while (it.hasNext())
			allMusic << it.next();

		// 🔐 als einzelner Blob speichern
		setConfigValue("music_index", allMusic.join("\n"));
	}

	
	

	Q_INVOKABLE QJsonArray filterMusic(const QString &pattern) {
		QJsonArray out;
		QString p = pattern.toLower();

		if (p.isEmpty())
			return out;

		struct Hit { QString full; QString shortName; int score; };
		QVector<Hit> hits;

		for (const QString &path : allMusic) {

			QString file = QFileInfo(path).fileName().toLower();
			QString full = path.toLower();

			int score = 0;

			// 1) Dateiname hat absolute Priorität
			if (file.contains(p)) {
				score += 1000;    // SEHR HOCH
				// Bonus: Treffer am Anfang des Namens
				if (file.startsWith(p)) score += 300;
			}

			// 2) kompletter Pfad (niedrige Relevanz)
			if (full.contains(p)) {
				score += 100;
			}

			if (score > 0) {
				hits.push_back({ path, shortenPath(path), score });
			}
		}

		// 3) Höchste Relevanz zuerst
		std::sort(hits.begin(), hits.end(),
			[](const Hit &a, const Hit &b) {
				return a.score > b.score;
			}
		);

		// 4) JSON zurückgeben
		int count = 0;
		for (const auto &h : hits) {
			QJsonObject o;
			o["full"] = h.full;
			o["short"] = h.shortName;
			out.append(o);
			if (++count >= 200) break;
		}

		return out;
	}


	/*Q_INVOKABLE void playMusic(const QString &file) {
		QProcess::startDetached("audacious", {"-E", file});
	}*/
	
	Q_INVOKABLE void playMusic(const QString &file) {

		QString player = getConfigValue("audio_player", "vlc");

		QStringList parts = player.split(' ', Qt::SkipEmptyParts);
		QString program = parts.takeFirst();

		QStringList args = parts;
		args << file;

		QProcess::startDetached(program, args);
	}
	
	Q_INVOKABLE void playVideo(const QString &file) {

		QString player = getConfigValue("video_player", "vlc");

		QStringList parts = player.split(' ', Qt::SkipEmptyParts);
		QString program = parts.takeFirst();

		QStringList args = parts;
		args << file;

		QProcess::startDetached(program, args);
	}
	
	
	
	Q_INVOKABLE void scanVideoDir() {

		QString root = getConfigValue("video_root", QDir::homePath() + "/Videos");
		allVideos.clear();

		QDirIterator it(
			root,
			QStringList()
				<< "*.mp4" << "*.MP4"
				<< "*.mkv" << "*.MKV"
				<< "*.webm" << "*.WEBM"
				<< "*.mov" << "*.MOV",
			QDir::Files,
			QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
		);

		while (it.hasNext())
			allVideos << it.next();

		setConfigValue("video_index", allVideos.join("\n"));
	}

	
	Q_INVOKABLE QJsonArray filterVideos(const QString &pattern) {
		QJsonArray out;
		QString p = pattern.toLower();

		if (p.isEmpty())
			return out;

		struct Hit { QString full; QString shortName; int score; };
		QVector<Hit> hits;

		for (const QString &path : allVideos) {

			QString file = QFileInfo(path).fileName().toLower();
			QString full = path.toLower();

			int score = 0;

			if (file.contains(p)) {
				score += 1000;
				if (file.startsWith(p)) score += 300;
			}

			if (full.contains(p))
				score += 100;

			if (score > 0)
				hits.push_back({ path, shortenPath(path), score });
		}

		std::sort(hits.begin(), hits.end(), [](auto &a, auto &b){
			return a.score > b.score;
		});

		int count = 0;
		for (auto &h : hits) {
			QJsonObject o;
			o["full"] = h.full;
			o["short"] = h.shortName;
			out.append(o);
			if (++count >= 200) break;
		}

		return out;
	}

	Q_INVOKABLE QJsonArray loadEventsForMonth(int year, int month) {
		QJsonArray arr;
		QSqlQuery q(notesDB);

		QString ym = QString("%1-%2-").arg(year).arg(month, 2, 10, QLatin1Char('0'));

		QString pattern = ym + "%";

		QString sql =
			"SELECT id, date, title, allday, start_time, end_time, repeat_yearly "
			"FROM events WHERE date LIKE '" + pattern + "'";

		//qDebug() << "SQL QUERY:" << sql;

		q.prepare(sql);
		q.exec();

		while (q.next()) {
			QJsonObject o;
			o["id"] = q.value(0).toInt();
			o["date"] = q.value(1).toString();
			o["title"] = q.value(2).toString();
			o["allday"] = (q.value(3).toInt() == 1);
			o["start"] = q.value(4).toString();
			o["end"] = q.value(5).toString();
			o["repeat"] = (q.value(6).toInt() == 1);
			arr.append(o);
		}

		return arr;
	}

	Q_INVOKABLE bool saveEvent(int id, QString date, QString title, bool allday, QString start, QString end, bool repeat){
		QSqlQuery q(notesDB);
		if (id == -1) {
			q.prepare("INSERT INTO events(date,title,allday,start_time,end_time,repeat_yearly) "
					  "VALUES (?,?,?,?,?,?)");
			q.addBindValue(date);
			q.addBindValue(title);
			q.addBindValue(allday ? 1 : 0);
			q.addBindValue(start);
			q.addBindValue(end);
			q.addBindValue(repeat ? 1 : 0);
		} else {
			q.prepare("UPDATE events SET title=?, allday=?, start_time=?, end_time=?, repeat_yearly=? "
					  "WHERE id=?");
			q.addBindValue(title);
			q.addBindValue(allday ? 1 : 0);
			q.addBindValue(start);
			q.addBindValue(end);
			q.addBindValue(repeat ? 1 : 0);
			q.addBindValue(id);
		}

		bool ok = q.exec();

		// Timer neu planen
		if (ok)
			scheduleTodayEventTimers();

		return ok;
	}

	Q_INVOKABLE bool deleteEvent(int id) {
		QSqlQuery q(notesDB);
		q.prepare("DELETE FROM events WHERE id=?");
		q.addBindValue(id);
		return q.exec();
	}
	
	Q_INVOKABLE void chooseFolder(const QString &configKey)
	{
		QString dir = QFileDialog::getExistingDirectory(
			nullptr,
			"Select folder",
			QDir::homePath(),
			QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
		);

		if (dir.isEmpty())
			return;

		setConfigValue(configKey, dir);
		emit folderChosen(configKey, dir);
	}
	
	Q_INVOKABLE QJsonArray checkDueEvents() {

		QJsonArray result;

		QDate today = QDate::currentDate();
		QTime now   = QTime::currentTime();

		QSqlQuery q(notesDB);
		q.exec(R"(
			SELECT id, date, title, allday, start_time, repeat_yearly
			FROM events
		)");

		while (q.next()) {

			int id = q.value(0).toInt();
			QDate evDate = QDate::fromString(q.value(1).toString(), "yyyy-MM-dd");
			QString title = q.value(2).toString();
			bool allday = q.value(3).toInt() == 1;
			QString startTimeStr = q.value(4).toString();
			bool repeat = q.value(5).toInt() == 1;

			bool isToday = repeat
				? (evDate.month() == today.month() &&
				   evDate.day()   == today.day())
				: (evDate == today);

			if (!isToday)
				continue;

			QString timePart = allday ? "allday" : startTimeStr;

			QString shownKey = QString("event_shown_%1_%2_%3")
				.arg(id)
				.arg(today.toString("yyyy-MM-dd"))
				.arg(timePart);

			if (getConfigValue(shownKey, "") == "1")
				continue;

			if (!allday) {
				QTime start = QTime::fromString(startTimeStr, "HH:mm");
				if (!start.isValid())
					continue;
					
				// DEBUG
				// qDebug() << "EVENT CHECK"
				//		 << "id:" << id
				//		 << "now:" << now.toString("HH:mm:ss.zzz")
				//		 << "start:" << start.toString("HH:mm");

				int nowMin   = now.hour() * 60 + now.minute();
				int startMin = start.hour() * 60 + start.minute();

				if (nowMin < startMin)
					continue;
			}


			QJsonObject o;
			o["id"] = id;
			o["title"] = title;
			o["allday"] = allday;
			o["start"] = startTimeStr;

			result.append(o);

			setConfigValue(shownKey, "1");
		}

		return result;
	}
	
	Q_INVOKABLE bool hasPendingEventOverlay() const {
		return pendingEventOverlay;
	}

	Q_INVOKABLE void clearPendingEventOverlay() {
		pendingEventOverlay = false;
	}
	

private:
	QStringList allMusic;
	QStringList allVideos;
	
	QString shortenPath(const QString &full) {
		QFileInfo fi(full);
		QString dir = fi.dir().dirName();
		return "…/" + dir + "/" + fi.fileName();
	}
	
	bool frontendIsReady = false;
	bool pendingEventOverlay = false;
	
	void scheduleTodayEventTimers() {

		QDate today = QDate::currentDate();
		QTime now   = QTime::currentTime();

		QSqlQuery q(notesDB);
		q.exec(R"(
			SELECT id, date, start_time, repeat_yearly
			FROM events
			WHERE allday = 0
		)");

		while (q.next()) {

			QDate evDate = QDate::fromString(q.value(1).toString(), "yyyy-MM-dd");
			QTime start  = QTime::fromString(q.value(2).toString(), "HH:mm");
			bool repeat  = q.value(3).toInt() == 1;

			bool isToday = repeat
				? (evDate.month() == today.month() && evDate.day() == today.day())
				: (evDate == today);

			if (!isToday || !start.isValid() || start <= now)
				continue;

			int ms = now.msecsTo(start);

			QTimer::singleShot(ms, this, [this]() {

				pendingEventOverlay = true;
				emit requestShow();

				// 🔁 Recheck-Timer, bis Event wirklich angezeigt wurde
				QTimer* retry = new QTimer(this);
				retry->setInterval(1000); // 1 Sekunde
				connect(retry, &QTimer::timeout, this, [this, retry]() {
					if (!pendingEventOverlay) {
						retry->stop();
						retry->deleteLater();
						return;
					}
					emit requestShow(); // triggert checkDueEvents erneut
				});
				retry->start();
			});


		}
	}


	
	
signals:
    void sendApps(const QJsonArray& arr);
    void requestToggle();
    void requestShow(); 
    void sendVolume(double v);
    void recordingStarted();
    void recordingStopped();
    void folderChosen(QString key, QString path);
    void frontendReadySignal();

    // jetzt verbunden mit SystemMonitor
    void sendSysInfo(double cpu, double ramUsed, double ramTotal,
                     double netUp = 0, double netDown = 0);
};

// ===============================
// Backend: screenshotPath()
// ===============================
QString Backend::screenshotPath() const {
    return QCoreApplication::applicationDirPath() + "/Screenshots/";
}

// ===============================
// Backend: recordPath()
// ===============================
QString Backend::recordPath() const {
    return QCoreApplication::applicationDirPath() + "/ScreenRecords/";
}


/* ===========================================================
   WINDOW (QWebEngine)
   =========================================================== */

class SvgWindow : public QWebEngineView {
    Q_OBJECT

public:
	CustomPage* customPage = nullptr;   // <-- hinzugefügt

    SvgWindow() {

		customPage = new CustomPage(this);
		setPage(customPage);

		backend = new Backend(this);

		// Plugins
		pluginManager = new PluginManager(this);

		// WebChannel + HTML setzen
		setupWebChannel();

		// ✅ 1) Plugins: JS/CSS erst injizieren wenn DOM wirklich geladen ist
		connect(page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
			if (!ok) return;
			if (pluginsInjected) return;
			pluginsInjected = true;
			pluginManager->injectAll(page());
		});

		// ✅ 2) Initialdaten: erst wenn Frontend wirklich bereit ist
		connect(backend, &Backend::frontendReadySignal,
				this, &SvgWindow::sendAppsToFrontend);

		// Toggle
		connect(backend, &Backend::requestToggle,
				this, &SvgWindow::toggle);
		
		connect(backend, &Backend::requestShow, this, [this]() {
			if (!visibleState) {
				animateShow();
			} else {
				// 🔥 Launcher ist bereits sichtbar → Overlay sofort prüfen
				if (backend->hasPendingEventOverlay()) {
					page()->runJavaScript(R"(
						backend.checkDueEvents().then(events => {
							if (events.length > 0) {
								events.forEach(ev => {
									uiAlert(`<h2>${ev.title}</h2>`, "Event");
								});
								backend.clearPendingEventOverlay();
							}
						});
					)");
				}
			}
		});

        

        
		loadConfig();

		setWindowFlags(Qt::FramelessWindowHint |
					   Qt::WindowStaysOnTopHint);

		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_AlwaysStackOnTop);
		setStyleSheet("background: transparent;");

		/* -------- WINDOW GEOMETRY -------- */
		QTimer::singleShot(0, this, [this]() {
			QPoint cursor = QCursor::pos();
			QScreen* s = QGuiApplication::screenAt(cursor);
			if (!s) s = QGuiApplication::primaryScreen();

			QRect r = s->geometry();
			setGeometry(r);
			panelHeight = r.height();
			move(0, -panelHeight);
		});

		/* -------- SOCKET (FIFO) -------- */
		createSocket();
		setupSocketWatcher();

		/* -------- SYSINFO VERBINDUNG -------- */
		connect(backend, &Backend::sendSysInfo,
				this,
				[this](double cpu, double ramUsed, double ramTotal, double cpuTemp, double)
		{
			QMetaObject::invokeMethod(this, [=]() {
				QString js = QString(
					"updateSysInfo(%1, %2, %3, %4);"
				).arg(cpu).arg(ramUsed).arg(ramTotal).arg(cpuTemp);

				page()->runJavaScript(js);
			}, Qt::QueuedConnection);
		});

	}

	
	

protected:
    void keyPressEvent(QKeyEvent* e) override {
		// ❗ STANDARDVERHALTEN → Event an JS weiterreichen
		QWebEngineView::keyPressEvent(e);
	}
    
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        // Rechtsklick vollständig blockieren
        event->accept();
        // nichts anzeigen
    }


private:
    Backend* backend;
    QJsonObject config;

    QPropertyAnimation* animPos = nullptr;
    bool visibleState = false;

    int sockfd = -1;
    QSocketNotifier* sockNotif = nullptr;
    int panelHeight = 0;

    QFileSystemWatcher* cssWatcher = nullptr;
    
    PluginManager* pluginManager = nullptr;
	bool pluginsInjected = false;
	
    /* ---------------------- WEBCHANNEL ---------------------- */
    void setupWebChannel() {
		QWebChannel* channel = new QWebChannel(this);
		GamepadManager* gamepad = new GamepadManager(this);
		pluginManager->setWebChannel(channel);

		// ✅ Plugin-APIs registrieren (früh, stabil)
		pluginManager->loadAll();
		
		channel->registerObject("backend", backend);
		channel->registerObject("gamepad", gamepad);
		channel->registerObject("plugins", pluginManager);

		page()->setWebChannel(channel);
		page()->setBackgroundColor(Qt::transparent);

		QString basePath = QCoreApplication::applicationDirPath() + "/";
		page()->setHtml(INDEX_HTML, QUrl::fromLocalFile(basePath));
	}


    /* ---------------------- CONFIG LOAD ---------------------- */
    void loadConfig() {
        QFile f("apps.json");
        f.open(QFile::ReadOnly);
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        config = doc.object();
        backend->config = config;
    }

    void sendAppsToFrontend() {
        QJsonArray apps = config["apps"].toArray();
        emit backend->sendApps(apps);
    }


    /* ---------------------- ANIMATIONS ---------------------- */
    void animateShow() {
        if (animPos) animPos->stop();

        move(0, -panelHeight);

        animPos = new QPropertyAnimation(this, "pos");
        animPos->setDuration(1);
        animPos->setStartValue(QPoint(0, -panelHeight));
        animPos->setEndValue(QPoint(0, 0));
        animPos->setEasingCurve(QEasingCurve::OutCubic);

        static bool firstLoad = true;
        bool* shownPtr = new bool(false);

        connect(animPos, &QPropertyAnimation::valueChanged,
                this, [this, shownPtr](const QVariant& v) {
            QPoint p = v.toPoint();

            if (!*shownPtr && p.y() > -panelHeight + 1) {
                *shownPtr = true;
                this->show();
                page()->runJavaScript("onWindowShown && onWindowShown();"); // Test
                
                if (backend) {
					QMetaObject::invokeMethod(backend, [this]() {
						
						if (backend->hasPendingEventOverlay()) {
							page()->runJavaScript(R"(
								backend.checkDueEvents().then(events => {
									if (events.length > 0) {
										events.forEach(ev => {
											uiAlert(`<h2>${ev.title}</h2>`, "Event");
										});
										backend.clearPendingEventOverlay();
									}
								});
							)");
						}
						
					}, Qt::QueuedConnection);
				}
                
                // ⭐ Filter zurücksetzen, damit Grid immer komplett sichtbar ist
				page()->runJavaScript("filterBuffer=''; applyFilter();");

                if (firstLoad) {
                    QCoreApplication::processEvents();
                    QThread::msleep(16);
                    QCoreApplication::processEvents();

                    //page()->load(QUrl::fromLocalFile(QDir::currentPath() + "/index.html"));

                    connect(page(), &QWebEnginePage::loadFinished,
                            this, [this](bool ok) {
                        if (ok) sendAppsToFrontend();
                    });

                    firstLoad = false;
                }
            }
        });

        animPos->start();
        visibleState = true;
        
    }

    void animateHide() {
        if (animPos) animPos->stop();

        animPos = new QPropertyAnimation(this, "pos");
        animPos->setDuration(1);
        animPos->setStartValue(QPoint(0, 0));
        animPos->setEndValue(QPoint(0, -panelHeight));
        animPos->setEasingCurve(QEasingCurve::InCubic);

        connect(animPos, &QPropertyAnimation::finished, this, [this]() {
			this->hide();
		});

        animPos->start();
        visibleState = false;
    }


    void toggle() {
        if (visibleState) animateHide();
        else animateShow();
    }


    /* ---------------------- SOCKET ---------------------- */
    void createSocket() {
        unlink(SOCKET_PATH);
        sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, SOCKET_PATH);

        bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    }

    void setupSocketWatcher() {
        sockNotif = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
        connect(sockNotif, &QSocketNotifier::activated,
                this,
            [this]() {
            char buf[32];
            recv(sockfd, buf, sizeof(buf), 0);

            if (QString(buf).trimmed() == "toggle")
                toggle();
        });
    }
};


/* ===========================================================
   MAIN
   =========================================================== */

class RecorderCleanup {
public:
    ~RecorderCleanup() {
        ::system("pkill -INT wf-recorder");
    }
};

static RecorderCleanup _recorderCleanup;


void handleStopSignals(int) {
    ::system("pkill -INT wf-recorder");
    _exit(0);
}

void installSignalHandlers() {
    signal(SIGTERM, handleStopSignals);
    signal(SIGINT,  handleStopSignals);
    signal(SIGHUP,  handleStopSignals);
    signal(SIGQUIT, handleStopSignals);
    signal(SIGABRT, handleStopSignals);
}

static void ensureMediaDirs()
{
    // Verzeichnis des laufenden Binaries
    QString baseDir = QCoreApplication::applicationDirPath();
    QDir dir(baseDir);

    // Screenshots
    if (!dir.exists("Screenshots")) {
        dir.mkpath("Screenshots");
    }

    // ScreenRecords
    if (!dir.exists("ScreenRecords")) {
        dir.mkpath("ScreenRecords");
    }
}



int main(int argc, char** argv) {
/*
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--no-sandbox "
        "--disable-gpu-sandbox "
        "--disable-seccomp-sandbox "
        "--disable-setuid-sandbox "
        "--disable-namespace-sandbox "
        "--disable-dev-shm-usage "
    );*/
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--log-level=3 --disable-logging");
    
    qputenv("QT_FILE_DIALOGS", "gtk");
    //qputenv("QT_NO_NATIVE_FILE_DIALOGS", "1");
	
	installSignalHandlers();
	
    QApplication app(argc, argv);
	
	// Doofe Debug Messages wegfiltern ("Error with Permissions-Policy header" ...)
	// installWebEngineStderrFilter();
	
	// ✅ Verzeichnisse sicherstellen
    ensureMediaDirs();
	
    SvgWindow w;
    w.hide();

    return app.exec();
}

#include "launcher.moc"
