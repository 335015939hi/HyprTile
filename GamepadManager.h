#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QString>
#include <QHash>
#include <QtGlobal>
#include <QFile>

/*
 * GamepadManager
 *
 * - Liest Linux /dev/input/js*
 * - Ermittelt GUID (SDL-kompatibel)
 * - Lädt Mapping aus gamecontrollerdb.txt
 * - Fallback auf einfache Defaults, wenn kein DB-Eintrag existiert
 * - Liefert abstrahierte Aktionen: up/down/left/right/confirm/back/menu
 */

class GamepadManager : public QObject {
    Q_OBJECT

public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager() override;

signals:
    // Zentrale, saubere API für HyprTile
    // pressed = true  -> gedrückt
    // pressed = false -> losgelassen
    void action(const QString& name, bool pressed);

private slots:
    void scanDevices();
    void onReadable();

private:
    /* ========= Low-Level Device Handling ========= */

    bool openFirstAvailable();
    void closeDevice();

    static QString linuxGuidFromJs(int fd);

    /* ========= gamecontrollerdb ========= */

    struct InputBinding {
        enum Type {
            Button,
            Axis,
            Hat
        } type = Button;

        int index = -1;   // Button- oder Axis-Index
        int hatMask = 0;  // nur bei Hat (z.B. h0.1)
    };

    bool loadMappingFromDB(const QString& guid);
    QString mapAction(const QString& dbName) const;

    /* ========= Event Handling ========= */

    void handleButton(int idx, bool pressed);
    void handleAxis(int idx, int value);

    void updateDigitalFromAxis(const QString& actionName, bool pressed);
    void fireRepeatable(const QString& actionName, bool pressed);

    /* ========= State ========= */

    int fd = -1;
    QString devPath;
    QString devName;
    QString devGuid;

    QSocketNotifier* notifier = nullptr;
    QTimer scanTimer;

    // DB-Mapping: action -> binding
    QHash<QString, InputBinding> bindings;

    // Digital state (für Axis/Hat → Button)
    QHash<QString, bool> digitalState;

    // Raw axis cache
    int axisRaw[32]{};

    /* ========= Timing / Feel ========= */

    int deadzone = 8000;        // js_event axis: -32767..32767
    int initialDelayMs = 220;   // optional (für spätere Repeats)
    int repeatMs = 80;

    /* ========= Fallback (nur wenn DB fehlt) ========= */

    void setupFallbackMapping();
};
