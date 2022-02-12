project (koules)
cmake_minimum_required (VERSION 3.16)

find_package (SDL REQUIRED)
find_package (SDL_mixer REQUIRED)
include_directories (${SDL_INCLUDE_DIR} ${SDLMIXER_INCLUDE_DIR})

add_executable (
    koules
    src/koules.c
    src/sound.c
    src/menu.c
    src/gameplan.c
    src/font.c
    src/intro.c
    src/sdl/SDL_prims.c
    src/sdl/init.c
    src/sdl/SFont.c
    src/framebuffer.c
    src/cmap.c
    src/rcfiles.c
    src/server.c
    src/client.c
    src/sock.c
    src/objectsio.c
)

add_custom_command(TARGET koules POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/sounds $<TARGET_FILE_DIR:koules>/sounds)

add_custom_command(TARGET koules POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/font $<TARGET_FILE_DIR:koules>/font)


add_definitions(-DSDL32BPP -DGCW -DSOUND)

target_link_libraries(koules ${SDL_LIBRARY} ${SDL_MIXER_LIBRARY} -lm)