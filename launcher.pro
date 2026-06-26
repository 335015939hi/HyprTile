QT += core gui webenginewidgets webchannel concurrent sql

CONFIG += c++17 console release
CONFIG += optimize_size no_rtti no_exceptions

TARGET = HyprTile

SOURCES += \
    launcher.cpp \
    SystemMonitor.cpp \
    WebScreenshotter.cpp \
    PluginManager.cpp \
    GamepadManager.cpp

HEADERS += \
    SystemMonitor.h \
    WebScreenshotter.h \
    PluginManager.h \
    WebPlugin.h \
    GamepadManager.h

RESOURCES += resources.qrc

# ==== Optimierungen für minimale Binary-Größe ====

# Optimize for size
QMAKE_CXXFLAGS += -Os

# Dead-code elimination
QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
QMAKE_LFLAGS   += -Wl,--gc-sections

# Sichtbarkeit runter
QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

# Kein RTTI, keine Exceptions
# (Qt nutzt beides nicht)
CONFIG += no_rtti no_exceptions

# LTO: Optional
# Du kannst es testen:
# 1. ohne LTO (bin oft kleiner!)
# 2. mit LTO (bin manchmal größer!)
#QMAKE_CXXFLAGS += -flto
#QMAKE_LFLAGS   += -flto

# Keine Symbole
QMAKE_LFLAGS += -s
