cmake_minimum_required(VERSION 3.25)
project(SunfleckDocumentation LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure with the Emscripten toolchain.")
endif()
find_package(Sunfleck 1.0.0 CONFIG REQUIRED)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
if(NOT Sunfleck_WEB_ASSETS)
  message(FATAL_ERROR "Set Sunfleck_DIR to the installed Wasm SDK CMake package.")
endif()
set(SUNFLECK_FLECKC "" CACHE FILEPATH "fleckc from the matching host SDK")
if(NOT EXISTS "${SUNFLECK_FLECKC}")
  message(FATAL_ERROR "Set SUNFLECK_FLECKC to the host SDK's fleckc executable.")
endif()
set(SUNFLECK_DOC_FONT "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSansCJKsc-Regular.otf" CACHE FILEPATH "Licensed CJK documentation font")
set(SUNFLECK_DOC_MONO "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSansMono.ttf" CACHE FILEPATH "Licensed monospaced documentation font")
foreach(font IN ITEMS SUNFLECK_DOC_FONT SUNFLECK_DOC_MONO)
  if(NOT EXISTS "${${font}}")
    message(FATAL_ERROR "Missing font ${${font}}. See assets/fonts/README.md or set ${font}.")
  endif()
endforeach()

file(GLOB_RECURSE pages CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/content/*.fleck"
  "${CMAKE_CURRENT_SOURCE_DIR}/components/*.fleck")
set(generated_pages "${CMAKE_CURRENT_BINARY_DIR}/pages.inc")
add_custom_command(OUTPUT "${generated_pages}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/pages.py" "${SUNFLECK_FLECKC}" "${generated_pages}"
  DEPENDS ${pages} shell.fleck sdk-links.json tools/pages.py tools/api_zh.py "${SUNFLECK_FLECKC}" VERBATIM)
add_custom_target(check_pages DEPENDS "${generated_pages}")

add_executable(documentation main.cpp "${generated_pages}")
target_link_libraries(documentation PRIVATE Sunfleck::sunfleck)
target_include_directories(documentation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set(web "${CMAKE_CURRENT_BINARY_DIR}/web")
set_target_properties(documentation PROPERTIES OUTPUT_NAME sunfleck SUFFIX .js
  RUNTIME_OUTPUT_DIRECTORY "${web}")
target_link_options(documentation PRIVATE --no-entry
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=524288 -sSTACK_OVERFLOW_CHECK=1
  "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','FS','UTF8ToString']"
  "--preload-file=${SUNFLECK_DOC_FONT}@/sunfleck/font.ttf"
  "--preload-file=${SUNFLECK_DOC_MONO}@/sunfleck/mono.ttf")
set_property(TARGET documentation APPEND PROPERTY LINK_DEPENDS "${SUNFLECK_DOC_FONT}" "${SUNFLECK_DOC_MONO}")

foreach(asset IN ITEMS index.html main.js)
  configure_file("${asset}" "${web}/${asset}" COPYONLY)
endforeach()
configure_file(assets/images/blue-marble.jpg "${web}/blue-marble.jpg" COPYONLY)
foreach(asset IN ITEMS host.js renderer.js text-input.js)
  configure_file("${Sunfleck_WEB_ASSETS}/${asset}" "${web}/${asset}" COPYONLY)
endforeach()
foreach(license IN ITEMS SheenBidi-LICENSE.txt Sunfleck-LICENSE.txt)
  configure_file("${Sunfleck_WEB_ASSETS}/${license}" "${web}/licenses/${license}" COPYONLY)
endforeach()
foreach(license IN ITEMS OFL-CJK.txt OFL-Mono.txt)
  configure_file("assets/fonts/${license}" "${web}/licenses/${license}" COPYONLY)
endforeach()
configure_file(deploy/README.txt "${web}/DEPLOYMENT.txt" COPYONLY)
configure_file(deploy/nginx.conf "${web}/nginx.example.conf" COPYONLY)
