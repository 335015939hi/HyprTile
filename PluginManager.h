#pragma once

#include <QObject>
#include <QVector>
#include <QPluginLoader>
#include <QStringList>
#include <QWebEnginePage>

#include "WebPlugin.h"

struct LoadedPlugin {
    QString id;
    QPluginLoader* loader;
    WebPlugin* iface;
};

class PluginManager : public QObject {
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);

    // Plugin discovery
    void loadAll();

    // JS / WebChannel API
    Q_INVOKABLE QStringList listPlugins() const;

    // 🔥 Inject JS / CSS from plugins into the page
    void injectAll(QWebEnginePage* page);

    // Wird vom Hauptprogramm gesetzt
    void setWebChannel(class QWebChannel* channel);

private:
    QVector<LoadedPlugin> plugins;
    QWebChannel* webChannel = nullptr;

    void loadPluginDir(const QString& path);
};
