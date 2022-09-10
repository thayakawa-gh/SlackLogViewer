function(deploy_qt PROJ_NAME)

find_package(Qt5 COMPONENTS Core)

get_target_property(_qmake_path Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_path ${_qmake_path} DIRECTORY)

if(WIN32)
    find_program(DEPLOYQT NAMES windeployqt HINTS "${_qt_bin_path}")
    add_custom_command(
        TARGET ${PROJ_NAME} POST_BUILD
        COMMAND ${DEPLOYQT} $<TARGET_FILE_DIR:${PROJ_NAME}>/$<TARGET_FILE_NAME:${PROJ_NAME}> -verbose=2 
            --$<IF:$<CONFIG:Debug>,debug,release> --dir "$<TARGET_FILE_DIR:${PROJ_NAME}>"
            --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime
            --no-webkit2 --no-angle --no-opengl-sw)
    install(CODE "execute_process(COMMAND ${DEPLOYQT} ${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${PROJ_NAME}> -verbose=2 
                                  --$<IF:$<CONFIG:Debug>,debug,release> --qmldir ${CMAKE_SOURCE_DIR} 
                                  --no-quick-import --no-translations --no-system-d3d-compiler --no-compiler-runtime 
                                  --no-webkit2 --no-angle --no-opengl-sw)")
elseif(APPLE)
    find_program(DEPLOYQT NAMES macdeployqt HINTS "${_qt_bin_path}")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${DEPLOYQT} ${CMAKE_BINARY_DIR}/${PROJ_NAME}.app $<IF:$<CONFIG:Debug>,-use-debug-lib,>)
    install(CODE "execute_process(COMMAND ${DEPLOYQT} ${CMAKE_BINARY_DIR}/${PROJ_NAME}.app $<IF:$<CONFIG:Debug>,-use-debug-lib,>")
endif()



endfunction()