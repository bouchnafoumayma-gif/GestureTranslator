QT += core gui widgets multimedia multimediawidgets sql
QT += sql
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    ../../Downloads/deux_doigts.webp \
    ../../Downloads/main_ouverte.webp \
    ../../Downloads/poing.jpg \
    ../../Downloads/trois_doigt.webp \
    ../../Downloads/un_doigt.webp \
    app.manifest

# Lien vers OpenCV
# Lien vers OpenCV MinGW
INCLUDEPATH += C:\opencv_mingw\OpenCV-MinGW-Build-OpenCV-4.5.5-x64\include

LIBS += -LC:\opencv_mingw\OpenCV-MinGW-Build-OpenCV-4.5.5-x64\x64\mingw\lib \
        -lopencv_core455 \
        -lopencv_imgproc455 \
        -lopencv_highgui455 \
        -lopencv_imgcodecs455 \
        -lopencv_video455 \
        -lopencv_videoio455 \
        -lopencv_objdetect455

RESOURCES += \
    resources.qrc


