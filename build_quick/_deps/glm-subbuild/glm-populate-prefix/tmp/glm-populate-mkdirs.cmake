# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-src"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-build"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/tmp"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/src"
  "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/root/.openclaw/workspace/SoftGPU/build_quick/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
