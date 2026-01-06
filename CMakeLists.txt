cmake_minimum_required(VERSION 3.19)
project(QSupportViewer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOPCH OFF)

# ▼▼▼ 手動設定用のルートパス定義 ▼▼▼
# ここにファイルがあることは確認済みです
set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")

# Qt のみ find_package で探す
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Core Concurrent)

# --- MPV 設定 (変更なし) ---
find_path(MPV_INCLUDE_DIR NAMES mpv/client.h HINTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mpv-dev/include")
find_library(MPV_LIBRARY NAMES mpv libmpv HINTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mpv-dev")

qt_standard_project_setup()

# --- ソースファイル定義 ---
set(PROJECT_SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui
    src/ui/compactwindow.cpp
    src/ui/compactwindow.h
    src/ui/dialogs/optionsdialog.cpp
    src/ui/dialogs/optionsdialog.h
    src/ui/dialogs/optionsdialog.ui
    src/ui/dialogs/aboutdialog.cpp
    src/ui/dialogs/aboutdialog.h
    src/ui/widgets/controlbar.cpp
    src/ui/widgets/controlbar.h
    src/ui/widgets/controlbar.ui
    src/ui/widgets/panoramaview.cpp
    src/ui/widgets/panoramaview.h
    src/ui/widgets/clickable_slider.cpp
    src/ui/widgets/clickable_slider.h
    src/ui/widgets/listoptionswidget.cpp
    src/ui/widgets/listoptionswidget.h
    src/ui/widgets/bookshelfwidget.cpp
    src/ui/widgets/bookshelfwidget.h
    src/ui/widgets/musicplaylistwidget.cpp
    src/ui/widgets/musicplaylistwidget.h
    src/ui/widgets/slideshowwidget.cpp
    src/ui/widgets/slideshowwidget.h
    src/ui/widgets/foldertreewidget.cpp
    src/ui/widgets/foldertreewidget.h
    src/logic/mediamanager.cpp
    src/logic/mediamanager.h
    src/logic/playlistmanager.cpp
    src/logic/playlistmanager.h
    src/logic/imageviewcontroller.cpp
    src/logic/imageviewcontroller.h
    src/logic/filescanner.cpp
    src/logic/filescanner.h
    src/logic/thememanager.cpp
    src/logic/thememanager.h
    src/logic/settingsmanager.cpp
    src/logic/settingsmanager.h
    src/logic/navigationmanager.cpp
    src/logic/navigationmanager.h
    src/utils/mediaitemdelegate.cpp
    src/utils/mediaitemdelegate.h
    src/utils/pixmap_object.cpp
    src/utils/pixmap_object.h
    src/utils/sdl_headers.h
    resources/resources.qrc
)

qt_add_executable(QSupportViewer WIN32 MACOSX_BUNDLE ${PROJECT_SOURCES}
    src/utils/common_types.h)

if(WIN32)
    target_link_libraries(QSupportViewer PRIVATE Shell32 dwmapi uxtheme)
endif()

# ▼▼▼ インクルードパスの強制指定 ▼▼▼
target_include_directories(QSupportViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/dialogs
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/src/logic
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${MPV_INCLUDE_DIR}
        # SDL2のインクルードパスを直接指定
        "${SDL2_ROOT}/include"
        "${SDL2_ROOT}/include/SDL2"
)

target_link_libraries(QSupportViewer
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Core
        Qt6::Concurrent

        # 順序変更: Mixer -> Main -> Core (DLLリンク用)
        # MinGWの一般的なリンク順序に合わせます
        "${SDL2_ROOT}/lib/libSDL2_mixer.dll.a"
        "${SDL2_ROOT}/lib/libSDL2main.a"
        "${SDL2_ROOT}/lib/libSDL2.dll.a"

        ${MPV_LIBRARY}
)

target_compile_definitions(QSupportViewer
    PRIVATE
        SDL_MAIN_HANDLED
        SDL_DISABLE_WINDOWS_CPUID
        HAVE_CPUID_H
)

include(GNUInstallDirs)

install(TARGETS QSupportViewer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QSupportViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
