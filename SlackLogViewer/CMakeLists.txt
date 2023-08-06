cmake_minimum_required(VERSION 3.14)

set(VER_MAJOR 1)
set(VER_MINOR 2)
set(VER_PATCH 3)
set(VER_REVISION 1)
set(COMPANY_COPYRIGHT "Copyright(C) 2020-2023 T.Hayakawa")

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
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(NOT QT_MAJOR_VERSION)
    set(QT_MAJOR_VERSION 6)
endif()

find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Core Gui Widgets Concurrent Network WebEngineWidgets REQUIRED)
find_package(QuaZip-Qt${QT_MAJOR_VERSION} REQUIRED)

file(GLOB _SOURCES *.cpp)
file(GLOB _NATVIS *.natvis)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${_SOURCES} ${_NATVIS} ${PruductVersionFiles})
elseif(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${_SOURCES})
endif()
add_definitions(-D_UNICODE -DUNICODE -DQAPPLICATION_CLASS=QApplication)

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_NO_KEYWORDS)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui 
    Qt${QT_MAJOR_VERSION}::Widgets Qt${QT_MAJOR_VERSION}::Concurrent 
    Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::WebEngineWidgets QuaZip::QuaZip)

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
    #set(RESOURCE_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents")
    set(INSTALL_RESOURCE_DIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app/Contents")
else()
    #set(RESOURCE_DIR "${CMAKE_BINARY_DIR}")
    set(INSTALL_RESOURCE_DIR "${CMAKE_INSTALL_PREFIX}")
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
		DESTINATION ${INSTALL_RESOURCE_DIR}/Resources)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PARALLEL_STL_ALGOS)
elseif (USE_TBB OR (NOT DEFINED USE_TBB AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11.0))
    #std::execution::par requires TBB for GCC
    find_package(TBB)
    if(TBB_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PARALLEL_STL_ALGOS)
        message(Parallel STL algorithms are enabled)
    endif()
endif()

#deploy_qt(${PROJECT_NAME})

if(APPLE)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND plutil -replace NSHighResolutionCapable -bool true ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Info.plist)
endif()

#if($<IF:$<CONFIG:Debug>, TRUE, FALSE>)
#    set(UPPER_BUILD_TYPE DEBUG)
#else()
#    set(UPPER_BUILD_TYPE RELEASE)
#endif()
#message("UPPER_BUILD_TYPE = ${UPPER_BUILD_TYPE}")

#find_package(Qt6 COMPONENTS Core)
get_target_property(_qmake_path Qt${QT_MAJOR_VERSION}::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_path ${_qmake_path} DIRECTORY)

#copy quazip dll
#get_target_property(QUAZIP_SHARED_LOC QuaZip::QuaZip IMPORTED_LOCATION_${UPPER_BUILD_TYPE})
if (WIN32)
    find_program(DEPLOYQT NAMES windeployqt HINTS "${_qt_bin_path}")
    #message("quazip shared loc = ${QUAZIP_SHARED_LOC}")
    #get_filename_component(QUAZIP_SHARED_NAME ${QUAZIP_SHARED_LOC} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:QuaZip::QuaZip> 
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:QuaZip::QuaZip>)
    install(FILES $<TARGET_FILE:QuaZip::QuaZip> DESTINATION ./)
    
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${DEPLOYQT} $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:QuaZip::QuaZip> -verbose=2 
            --$<IF:$<CONFIG:Debug>,debug,release> --dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime
            --no-opengl-sw)
    install(CODE "execute_process(COMMAND ${DEPLOYQT} ${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:QuaZip::QuaZip> -verbose=2 
                                  --$<IF:$<CONFIG:Debug>,debug,release>  
                                  --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime 
                                  --no-opengl-sw)")
endif()

#deployqt
if(WIN32)
    find_program(DEPLOYQT NAMES windeployqt HINTS "${_qt_bin_path}")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${DEPLOYQT} $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:${PROJECT_NAME}> -verbose=2 
            --$<IF:$<CONFIG:Debug>,debug,release> --dir $<TARGET_FILE_DIR:${PROJECT_NAME}>
            --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime
            --no-opengl-sw)
    install(CODE "execute_process(COMMAND cmd /c echo ${DEPLOYQT} $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR})")
    install(CODE "execute_process(COMMAND ${DEPLOYQT} ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe -verbose=2 
                                  --$<IF:$<CONFIG:Debug>,debug,release> 
                                  --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime 
                                  --no-opengl-sw)")
elseif(APPLE)
    find_program(DEPLOYQT NAMES macdeployqt HINTS "${_qt_bin_path}")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${DEPLOYQT} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app $<IF:$<CONFIG:Debug>,-use-debug-lib,>)
    install(CODE "execute_process(COMMAND ${DEPLOYQT} ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app $<IF:$<CONFIG:Debug>,-use-debug-lib,>)")
endif()

#copy zlib dll
if(WIN32 AND DEFINED ZLIB_SHARED)
    get_filename_component(ZLIB_SHARED_NAME ${ZLIB_SHARED} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${ZLIB_SHARED}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${ZLIB_SHARED_NAME})
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
