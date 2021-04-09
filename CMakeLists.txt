cmake_minimum_required(VERSION 3.19)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_FLAGS  -Wall)
set(CMAKE_CXX_FLAGS  -Wall)
set(CMAKE_C_FLAGS_RELEASE  -s)
set(CMAKE_CXX_FLAGS_RELEASE  -s)
set(LIBS_DIR "$ENV{HOME}/$ENV{DOCKERUSER}/.leila/lib")
set(QT_DIR "$ENV{HOME}/$ENV{DOCKERUSER}/Qt/5")

SET(PROJECT_NAME ScreenController)
PROJECT(${PROJECT_NAME})

IF( MATCHES Windows)
    set(_MACHINE_ARCH_FLAG /machine:x86)
    ENDIF( MATCHES Windows)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS  -std=c++17)

FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
#FIND_PACKAGE(QX11Info REQUIRED)

IF(UNIX)
FIND_PACKAGE(Qt5X11Extras REQUIRED)
#sudo apt-get install -y libqt5x11extras5-dev
ELSEIF(WIN32)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
ENDIF(UNIX)

INCLUDE_DIRECTORIES(
    ${QT_DIR}/Src/qtvirtualkeyboard/tests/manual/
    ${LIBS_DIR}/turbojpeg/build
    ${LIBS_DIR}/turbojpeg
    ${LIBS_DIR}/openssl/include
    ${LIBS_DIR}/libwebp/src
    ../License
    ../Updater
    DiscoveryService
    .
)

# this will run uic on .ui files:
QT5_WRAP_UI(PROJECT_UI_HDRS
    ../License/LicenseWidget.ui
    ../Updater/UpdaterWidget.ui
    CrossCursorWidget.ui
    MainWindow.ui
    NodeNameDialog.ui
    TransparentMaximizedWindow.ui
)
QT5_ADD_RESOURCES(PROJECT_RESOURCES
    screencontroller.qrc
)
ADD_DEFINITIONS(
    -DHAVE_STDINT_H
    -DHAVE_LIBJPEG
)

SET(SoundLibs )
SET(SSLLibs )
SET(X11Libs )
IF(UNIX)
    LINK_DIRECTORIES(
	${LIBS_DIR}/libturbojpeg/build/
	${LIBS_DIR}/libwebp/build/
	${CMAKE_BUILD_DIR}/x265/source/
#        ${LIBS_DIR}/libxdo/
    )
    SET(SSLLibs
        crypto
        ssl
        dl
    )
    SET(X11Libs
        X11
        Xfixes #/usr/lib/x86_64-linux-gnu/libXfixes.so.3.1.0
	Xtst
	Qt5X11Extras #ln -s /home/developer/oosman/Qt5.12.1/5.12.1/gcc_64/lib/libQt5X11Extras.so ~/work/libs/build/linux/bin/libQt5X11Extras.so
#        xdo
    )
#ln -s /usr/lib/x86_64-linux-gnu/libpulse-mainloop-glib.so.0.0.5
    SET(JPEGLib
        turbojpeg
        jpeg
        )
    SET(PROJECT_LINUX_SOURCES
        X11Mouse.cpp
        X11Mouse.h
        X11Key.cpp
        X11Key.h
        X11WindowEnumerator.h
        X11WindowEnumerator.cpp
    )
    ADD_DEFINITIONS(
        -DLinux
    )
ENDIF(UNIX)

SET(PROJECT_SOURCES
    ${PROJECT_LINUX_SOURCES}
    ../License/License.cpp
    ../License/License.h
    ../License/LicenseComm.cpp
    ../License/LicenseComm.h
    ../License/LicenseWidget.cpp
    ../License/LicenseWidget.h
    ../License/NetworkRequest.cpp
    ../License/NetworkRequest.h
    ../Updater/UpdaterWidget.cpp
    ../Updater/UpdaterWidget.h
    Command.cpp
    Command.h
    CommandMessage.cpp
    CommandMessage.h
    EventHandler.cpp
    EventHandler.h
    Frame.h
    FrameExtractor.h
    FrameExtractor.cpp
    ImageConverterInterface.h
    JpegConverter.cpp
    JpegConverter.h
    KeyInterface.cpp
    KeyInterface.h
    MainWindow.cpp
    MainWindow.h
    MouseInterface.cpp
    MouseInterface.h
    NodeListView.cpp
    NodeListView.h
    NodeModel.cpp
    NodeModel.h
    NodeNameDialog.cpp
    NodeNameDialog.h
    RegionMapper.cpp
    RegionMapper.h
    ScreenStreamer.cpp
    ScreenStreamer.h
    SocketTrasceiver.cpp
    SocketTrasceiver.h
    Stats.cpp
    Stats.h
    TransparentMaximizedWindow.cpp
    TransparentMaximizedWindow.h
    WebPConverter.cpp
    WebPConverter.h
    WindowEnumeratorInterface.h
    main.cpp
    X265Converter.h
    X265Converter.cpp
)

IF(UNIX)
    SET(CMAKE_INSTALL_RPATH /usr/local/bin/kingfisher-screen-controller)
ENDIF(UNIX)

ADD_SUBDIRECTORY(DiscoveryService)
ADD_SUBDIRECTORY(x265)
ADD_SUBDIRECTORY(libde265)

ADD_EXECUTABLE(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
    ${App_ICON}
    ${PROJECT_SOURCES}
    ${PROJECT_MOC_SRCS}
    ${PROJECT_UI_HDRS}
    ${PROJECT_RESOURCES}
)
#MESSAGE(STATUS "Baah Hum Bug!")

TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    ${Qt5Widgets_LIBRARIES}
    Qt5::Network
    ${SSLLibs}
    ${JPEGLib}
    ${X11Libs}
    pthread
    discovery.lib
    imageioutil
    imagedec
    imageenc
    webp
    webpmux
    webpdecoder
    exampleutil
    webpdemux
    x265
    dec265
)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES LINK_FLAGS -L/usr/local/bin/oxpecker-screen-controller/)
    execute_process(COMMAND bash "-c" "/bin/ln -sf ${QT_DIR}/gcc_64/plugins/platforms platforms"
    OUTPUT_VARIABLE LN_CMD_RES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")


