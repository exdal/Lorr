cmake_minimum_required(VERSION 3.6)

PROJECT (tinygltf)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

SET(CMAKE_CXX_STANDARD 11)

option(TINYGLTF_HEADER_ONLY "On: header-only mode. Off: create tinygltf library(No TINYGLTF_IMPLEMENTATION required in your project)" OFF)
option(TINYGLTF_INSTALL "Install tinygltf files during install step. Usually set to OFF if you include tinygltf through add_subdirectory()" ON)

#
# for add_subdirectory and standalone build
#
if (TINYGLTF_HEADER_ONLY)
  add_library(tinygltf INTERFACE)

  target_include_directories(tinygltf
          INTERFACE
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

else (TINYGLTF_HEADER_ONLY)
  add_library(tinygltf)
  target_sources(tinygltf PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/tiny_gltf.cc)
  target_include_directories(tinygltf
          INTERFACE
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
          )
endif (TINYGLTF_HEADER_ONLY)

if (TINYGLTF_INSTALL)
  install(TARGETS tinygltf EXPORT tinygltfTargets)
  install(EXPORT tinygltfTargets NAMESPACE tinygltf:: FILE TinyGLTFTargets.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
  configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/TinyGLTFConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/TinyGLTFConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TinyGLTFConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
  # Do not install .lib even if !TINYGLTF_HEADER_ONLY

  INSTALL ( FILES
    json.hpp
    stb_image.h
    stb_image_write.h
    tiny_gltf.h
    ${TINYGLTF_EXTRA_SOUECES}
    DESTINATION
    include
    )

endif(TINYGLTF_INSTALL)
