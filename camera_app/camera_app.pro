QT += core gui widgets multimedia multimediawidgets opengl openglwidgets

CONFIG += c++17

TARGET = camera_app
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    frangiglwidget.cpp

HEADERS += \
    mainwindow.h \
    frangiglwidget.h

# Правила по умолчанию для развертывания
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target