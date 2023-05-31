cmake_minimum_required(VERSION 3.10)

include(GNUInstallDirs)

project(filevars
	VERSION 0.1
    DESCRIPTION "Server to map variables to template files"
)

find_library( LIB_VARSERVER varserver REQUIRED )
find_library( LIB_TJSON tjson REQUIRED )

add_executable( ${PROJECT_NAME}
	src/filevars.c
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE inc
)

target_link_libraries( ${PROJECT_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	rt
	${LIB_VARSERVER}
    ${LIB_TJSON}
)

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

file(GLOB test_files "test/*.tmpl")

install( FILES ${test_files}
	DESTINATION /usr/share/templates
)