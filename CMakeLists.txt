cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED BUILDTARGET)
  message(WARNING "Please define BUILDTARGET to choose your target platform: [desktop, vita]. Defaulting to vita.")
  set(BUILDTARGET "vita")
endif()

if (BUILDTARGET STREQUAL "vita")
  if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
      set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
      message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
  endif()
endif()

project(sdlsand)

if (BUILDTARGET STREQUAL "vita")
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1 -d ATTRIBUTE2=12")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14 -Wno-narrowing -O3")

include_directories(.)
add_executable(${PROJECT_NAME} main.cpp CmdLine.cpp)

if (BUILDTARGET STREQUAL "vita")
  find_package(SDL2 REQUIRED)
  target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2-static
    ScePower_stub
    SceDisplay_stub
    SceCtrl_stub
    SceAudio_stub
    SceSysmodule_stub
    SceGxm_stub
    SceCommonDialog_stub
    SceAppMgr_stub
    SceTouch_stub
    SceHid_stub
    SceMotion_stub
    m
  )
else()
  find_package(SDL2 REQUIRED HINTS /usr/local/Cellar/sdl2/2.0.20/)
  include_directories(${SDL2_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()

if (BUILDTARGET STREQUAL "vita")
  vita_create_self(eboot.bin ${PROJECT_NAME})
  vita_create_vpk(sdlsand.vpk "SDLSAND01" eboot.bin
    VERSION "01.00"
    NAME "SDL2Sand"
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/template.xml sce_sys/livearea/contents/template.xml
  )
endif()