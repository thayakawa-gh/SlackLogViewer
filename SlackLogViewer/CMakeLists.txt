cmake_minimum_required(VERSION 3.14)

set(VER_MAJOR 1)
set(VER_MINOR 1)
set(VER_PATCH 2)
set(VER_REVISION 4)
set(COMPANY_COPYRIGHT "Copyright(C) 2020-2022 T.Hayakawa")

project(SlackLogViewer VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}.${VER_REVISION}")

if(WIN32)
    include(GenerateProductVersion.cmake)

    generate_product_version(PruductVersionFiles
        NAME "SlackLogViewer"
        FILE_DESCRIPTION SlackLogViewer
        VERSION_MAJOR ${VER_MAJOR}
        VERSION_MINOR ${VER_MINOR}
        VERSION_PATCH ${VER_PATCH}
        VERSION_REVISION ${VER_REVISION}
        COMPANY_COPYRIGHT ${COMPANY_COPYRIGHT}
        ORIGINAL_FILENAME SlackLogViewer.exe
        INTERNAL_NAME SlackLogViewer.exe)
else()
    configure_file("Version.h.in" "Version.h")
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
endif()

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /W4 /utf-8
        /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        /D_WIN32_WINNT=0x0602 /DWINVER=0x0602)
else()
    add_compile_options(-Wall)
endif()


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui Widgets Concurrent Network WebEngineWidgets REQUIRED)
find_package(QuaZip-Qt5 REQUIRED)

file(GLOB _SOURCES *.cpp)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${_SOURCES} ${PruductVersionFiles})
elseif(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${_SOURCES})
endif()
add_definitions(-D_UNICODE -DUNICODE -DQAPPLICATION_CLASS=QApplication)

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_NO_KEYWORDS)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent Qt5::Network Qt5::WebEngineWidgets QuaZip::QuaZip)

if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE -static-libstdc++ -static-libgcc)
endif()

install(TARGETS ${PROJECT_NAME}
        EXPORT ${MY_PACKAGE_NAME}Targets
        BUNDLE DESTINATION ./
        RUNTIME DESTINATION ./)

include(DeployQt.cmake)

if(APPLE)
set(RESOURCE_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/MacOS")
else()
set(RESOURCE_DIR "${CMAKE_BINARY_DIR}")
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
    ${RESOURCE_DIR}/Resources/)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
		DESTINATION Resources)
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${RESOURCE_DIR}/Cache/)
install(DIRECTORY DESTINATION "Cache")

# std::execution::par might require TBB
find_package(TBB)
if(TBB_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
endif()

deploy_qt(${PROJECT_NAME})

if(APPLE)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND plutil -replace NSHighResolutionCapable -bool true ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Info.plist)
endif()

string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_BUILD_TYPE)

#copy quazip dll
get_target_property(QUAZIP_SHARED_LOC QuaZip::QuaZip IMPORTED_LOCATION_${UPPER_BUILD_TYPE})
if (WIN32 AND QUAZIP_SHARED_LOC)
    get_filename_component(QUAZIP_SHARED_NAME ${QUAZIP_SHARED_LOC} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${QUAZIP_SHARED_LOC}
        ${CMAKE_CURRENT_BINARY_DIR}/${QUAZIP_SHARED_NAME})
    install(FILES ${QUAZIP_SHARED_LOC} DESTINATION ./)
endif()

#copy zlib dll
if(WIN32 AND DEFINED ZLIB_SHARED)
    get_filename_component(ZLIB_SHARED_NAME ${ZLIB_SHARED} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${ZLIB_SHARED}
        ${CMAKE_CURRENT_BINARY_DIR}/${ZLIB_SHARED_NAME})
    install(FILES ${ZLIB_SHARED} DESTINATION ./)
endif()

#copy openssl dll
if(WIN32 AND DEFINED OPENSSL_DIR)
    file(GLOB LIBSSL_SHARED "${OPENSSL_DIR}/libssl*.dll")
    get_filename_component(LIBSSL_SHARED_NAME ${LIBSSL_SHARED} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBSSL_SHARED}
        ${CMAKE_CURRENT_BINARY_DIR}/${LIBSSL_SHARED_NAME})
    install(FILES ${LIBSSL_SHARED} DESTINATION ./)

    file(GLOB LIBCRYPTO_SHARED "${OPENSSL_DIR}/libcrypto*.dll")
    get_filename_component(LIBCRYPTO_SHARED_NAME ${LIBCRYPTO_SHARED} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBCRYPTO_SHARED}
        ${CMAKE_CURRENT_BINARY_DIR}/${LIBCRYPTO_SHARED_NAME})
    install(FILES ${LIBCRYPTO_SHARED} DESTINATION ./)
endif()
