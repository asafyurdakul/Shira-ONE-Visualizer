QT       += core gui opengl concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

LIBS += -lOpenGL32 -lglu32

SOURCES += \
    main.cpp \
    oneloader.cpp \
    onereader.cpp \
    onerenderer.cpp

HEADERS += \
    oneloader.h \
    onereader.h \
    onerenderer.h


FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    shaders.qrc

