function(hid_mouse_add_winuhid target_name)
  set(WINUHID_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/WinUHid")
  set(WINUHID_DEVS_PROJECT "${WINUHID_ROOT}/WinUHidDevs/WinUHidDevs.vcxproj")

  if(NOT EXISTS "${WINUHID_DEVS_PROJECT}")
    message(FATAL_ERROR
      "WinUHid submodule is missing. Run: git submodule update --init --recursive")
  endif()

  if(NOT CMAKE_VS_MSBUILD_COMMAND)
    message(FATAL_ERROR "A Visual Studio generator with MSBuild is required")
  endif()

  add_custom_target(winuhid-userspace
    COMMAND "${CMAKE_VS_MSBUILD_COMMAND}"
      "${WINUHID_DEVS_PROJECT}"
      /nologo
      /m
      /t:Build
      /p:Configuration=$<CONFIG>
      /p:Platform=x64
      "/p:SolutionDir=${WINUHID_ROOT}/"
    COMMENT "Building WinUHid user-mode SDK and mouse helpers"
    VERBATIM
  )

  add_dependencies(${target_name} winuhid-userspace)

  set(WINUHID_OUTPUT_DIR "${WINUHID_ROOT}/build/$<CONFIG>/x64")
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${WINUHID_OUTPUT_DIR}/WinUHid.dll"
      "$<TARGET_FILE_DIR:${target_name}>/WinUHid.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${WINUHID_OUTPUT_DIR}/WinUHidDevs.dll"
      "$<TARGET_FILE_DIR:${target_name}>/WinUHidDevs.dll"
    COMMENT "Copying WinUHid user-mode DLLs next to hid-mouse"
    VERBATIM
  )
endfunction()
