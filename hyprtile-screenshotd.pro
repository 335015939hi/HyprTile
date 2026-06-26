QT += core gui webenginewidgets sql

TEMPLATE = app
TARGET = hypertile-screenshotd

CONFIG += c++17 console release
CONFIG += optimize_size no_rtti no_exceptions

SOURCES += \
    hypertile-screenshotd.cpp \
    WebScreenshotter.cpp

HEADERS += \
    WebScreenshotter.h

# ================================
# Optimierungen für minimale Größe
# ================================

# Optimize for size
QMAKE_CXXFLAGS += -Os

# Dead-code elimination
QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
QMAKE_LFLAGS   += -Wl,--gc-sections

# Visibility runter
QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

# Keine Symbole
QMAKE_LFLAGS += -s
