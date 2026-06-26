#include "PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QWebChannel>
#include <QWebEnginePage>

/* ===============================
   Constructor
   =============================== */

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

/* ===============================
   Public API
   =============================== */

void PluginManager::setWebChannel(QWebChannel* channel)
{
    webChannel = channel;
}

void PluginManager::loadAll()
{
    QDir pluginsDir(QCoreApplication::applicationDirPath() + "/plugins");
    if (!pluginsDir.exists()) {
        qDebug() << "[PluginManager] No plugins directory";
        return;
    }

    for (const QString& sub :
         pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        loadPluginDir(pluginsDir.absoluteFilePath(sub));
    }
}

QStringList PluginManager::listPlugins() const
{
    QStringList out;
    for (const auto& p : plugins)
        out << p.id;
    return out;
}

/* ===============================
   Internal
   =============================== */

void PluginManager::loadPluginDir(const QString& path)
{
    QDir dir(path);

    for (const QString& file : dir.entryList(QDir::Files)) {

        if (!file.endsWith(".so"))
            continue;

        auto* loader = new QPluginLoader(dir.absoluteFilePath(file), this);
        QObject* obj = loader->instance();

        if (!obj) {
            qWarning() << "[PluginManager] Plugin load failed:"
                       << loader->errorString();
            delete loader;
            continue;
        }

        auto* plugin = qobject_cast<WebPlugin*>(obj);
        if (!plugin) {
            qWarning() << "[PluginManager] Invalid plugin (no WebPlugin):"
                       << file;
            loader->unload();
            delete loader;
            continue;
        }

        QObject* api = plugin->apiObject();
        if (!api) {
            qWarning() << "[PluginManager] Plugin returned NULL apiObject():"
                       << plugin->id();
            loader->unload();
            delete loader;
            continue;
        }

        // 🔥 WICHTIG: Plugin-API DIREKT im WebChannel registrieren
        if (webChannel) {
            webChannel->registerObject(plugin->id(), api);
            qDebug() << "[PluginManager] Registered plugin API:"
                     << plugin->id()
                     << "(" << api->metaObject()->className() << ")";
        } else {
            qWarning() << "[PluginManager] No WebChannel set – cannot register plugin:"
                       << plugin->id();
        }

        plugins.push_back({
            plugin->id(),
            loader,
            plugin
        });

        qDebug() << "[PluginManager] Loaded plugin:" << plugin->id();
    }
}

void PluginManager::injectAll(QWebEnginePage* page)
{
    if (!page)
        return;

    for (const auto& p : plugins) {

        WebPlugin* plugin = p.iface;
        if (!plugin)
            continue;

        // ---------- CSS ----------
        const QString css = plugin->css();
        if (!css.isEmpty()) {
            page->runJavaScript(
                "(function(){"
                "const s=document.createElement('style');"
                "s.setAttribute('data-plugin','" + plugin->id() + "');"
                "s.textContent=`" + css + "`;"
                "document.head.appendChild(s);"
                "})();"
            );
        }

        // ---------- JS ----------
        const QString js = plugin->js();
        if (!js.isEmpty()) {
            page->runJavaScript(js);
        }
    }
}

