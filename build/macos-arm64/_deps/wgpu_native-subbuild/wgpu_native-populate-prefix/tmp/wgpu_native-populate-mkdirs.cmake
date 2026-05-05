# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-src")
  file(MAKE_DIRECTORY "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-src")
endif()
file(MAKE_DIRECTORY
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-build"
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix"
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/tmp"
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/src/wgpu_native-populate-stamp"
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/src"
  "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/src/wgpu_native-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/src/wgpu_native-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/anderian/projects/Dexilate/build/macos-arm64/_deps/wgpu_native-subbuild/wgpu_native-populate-prefix/src/wgpu_native-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
