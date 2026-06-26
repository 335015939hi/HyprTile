#pragma once
#include <QObject>
#include <QString>

class WebPlugin {
public:
    virtual ~WebPlugin() = default;

    // eindeutige ID → window.{id}
    virtual QString id() const = 0;

    // QObject mit Q_INVOKABLE APIs
    virtual QObject* apiObject() = 0;

    // optional: API-Version
    virtual int apiVersion() const { return 1; }

    // 🔥 OPTIONAL: JS/CSS Injection
    virtual QString js() const { return {}; }
    virtual QString css() const { return {}; }
};

#define WebPlugin_iid "org.hyprtile.WebPlugin"
Q_DECLARE_INTERFACE(WebPlugin, WebPlugin_iid)
