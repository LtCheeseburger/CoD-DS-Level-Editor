# =============================================================================
#  libs/nitro_gx/CMakeLists.txt
#  Static library: NDS GX display list decoder + TEX0 parser + OpenGL renderer
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# Find OpenGL (required for NsbmdTextureRenderer)
find_package(OpenGL REQUIRED)

set(NITRO_GX_SOURCES
    src/Tex0Parser.cpp
    src/GxDisplayListDecoder.cpp
    src/NsbmdTextureRenderer.cpp
)

set(NITRO_GX_HEADERS
    include/NitroTexture.hpp
    include/Tex0Parser.hpp
    include/DecodedNsbmdMesh.hpp
    include/GxDisplayListDecoder.hpp
    include/NsbmdTextureRenderer.hpp
)

add_library(nitro_gx STATIC ${NITRO_GX_SOURCES} ${NITRO_GX_HEADERS})

target_include_directories(nitro_gx
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nitro_gx
    PUBLIC  OpenGL::GL
)

target_compile_features(nitro_gx PUBLIC cxx_std_20)

# In the root CMakeLists.txt add:
#   add_subdirectory(libs/nitro_gx)
# In your app target:
#   target_link_libraries(OC_Shell PRIVATE nitro_gx)
