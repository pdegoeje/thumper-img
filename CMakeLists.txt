cmake_minimum_required(VERSION 3.20)

project(thumper-img VERSION 1.17.0 LANGUAGES CXX C)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui Widgets QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

if(WIN32)
  set(EXTRA_SOURCES thumper.rc)
endif()

qt_add_executable(thumper
  dct/fast-dct-lee.c
  dct/fast-dct-lee.h
  fileutils.cpp
  fileutils.h
  imagedao.cpp
  imagedao.h
  imagemetadata.cpp
  imagemetadata.h
  imageprocessor.cpp
  imageprocessor.h
  imageref.cpp
  imageref.h
  main.cpp
  simpleset.h
  sqlite3.c
  sqlite3.h
  sqlitehelper.cpp
  sqlitehelper.h
  taglist.cpp
  taglist.h
  thumper.cpp
  thumper.h
  thumperimageprovider.cpp
  thumperimageprovider.h
  ${EXTRA_SOURCES}
)

qt_add_qml_module(thumper
  URI thumper
  QML_FILES
    DottedBorder.qml
    Lightbox.qml
    Settings.qml
    Tag.qml
    TagList.qml
    TagSelection.qml
    main.qml
  RESOURCES
    baseline_drag_indicator_white_24dp.png
    baseline_folder_open_white_24dp.png
    baseline_save_alt_white_24dp.png
    baseline_settings_white_18dp.png
    checkboard.png
    eye.png
    selection_box.png
)

qt_add_resources(thumper "CONTROLS_CONF"
  PREFIX "/"
  FILES qtquickcontrols2.conf
)

target_link_libraries(thumper PRIVATE Qt::Quick Qt::Gui Qt::Widgets Qt::QuickControls2)

qt_generate_deploy_app_script(
  TARGET thumper
  OUTPUT_SCRIPT deploy_script
  NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
