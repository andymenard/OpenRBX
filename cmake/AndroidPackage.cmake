# Android packaging for the client. Included from the top-level CMakeLists.txt once the roblox target
# exists. Qt's androiddeployqt and Gradle turn it into an APK: `cmake --build <dir> --target roblox_make_apk`.

set(OPENRBX_ANDROID_CONTENT_DIR "" CACHE PATH
  "Game content directory to package into the APK. Left empty, content is pushed to the device separately.")

set(OPENRBX_ANDROID_PACKAGE_DIR "${PROJECT_BINARY_DIR}/android-package")
set(OPENRBX_ANDROID_ASSET_DIR "${OPENRBX_ANDROID_PACKAGE_DIR}/assets/openrbx")

# Runtime files that sit beside the executable on desktop builds. They are copied to the app's data
# directory on first launch; the stamp makes each newly built APK replace them.
set(OPENRBX_ANDROID_STAGE_COMMANDS
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENRBX_ANDROID_ASSET_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${OPENRBX_RUNTIME_OUTPUTS} "${OPENRBX_ANDROID_ASSET_DIR}"
  # The place the launcher opens, with the script that starts single-player play in it.
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENRBX_ANDROID_ASSET_DIR}/places"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${PROJECT_SOURCE_DIR}/Client/qt/android/places/obby.rbxl"
          "${PROJECT_SOURCE_DIR}/Client/qt/android/places/solo.lua"
          "${OPENRBX_ANDROID_ASSET_DIR}/places"
  COMMAND "${CMAKE_COMMAND}" "-DSTAMP=${OPENRBX_ANDROID_ASSET_DIR}/stamp.txt" -P
          "${PROJECT_SOURCE_DIR}/cmake/AndroidStamp.cmake"
)
if (OPENRBX_ANDROID_CONTENT_DIR)
  list(APPEND OPENRBX_ANDROID_STAGE_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OPENRBX_ANDROID_CONTENT_DIR}" "${OPENRBX_ANDROID_ASSET_DIR}/content"
  )
else()
  message(STATUS "OPENRBX_ANDROID_CONTENT_DIR is not set; the APK will not include game content")
endif()
# Original projectile models fill the missing built-in tool content in the Android package.
list(APPEND OPENRBX_ANDROID_STAGE_COMMANDS
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENRBX_ANDROID_ASSET_DIR}/content/fonts"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${PROJECT_SOURCE_DIR}/Client/qt/android/content/fonts/Rocket.rbxm"
          "${PROJECT_SOURCE_DIR}/Client/qt/android/content/fonts/SlingshotPellet.rbxm"
          "${OPENRBX_ANDROID_ASSET_DIR}/content/fonts"
)
add_custom_target(roblox_android_assets ALL ${OPENRBX_ANDROID_STAGE_COMMANDS} DEPENDS runtime_files VERBATIM)
add_dependencies(roblox roblox_android_assets)

# Shared libraries built here that the client loads. androiddeployqt finds Qt's own libraries.
# Plain paths: Qt splits this property on ':' when it writes the deployment settings, which breaks
# generator expressions. All three targets put their library in the top of the build directory.
set(OPENRBX_ANDROID_EXTRA_LIBS)
foreach(lib gl4es openal rgmain)
  list(APPEND OPENRBX_ANDROID_EXTRA_LIBS "${PROJECT_BINARY_DIR}/lib${lib}.so")
endforeach()
add_dependencies(roblox GL OpenAL rgmain)

set_target_properties(roblox PROPERTIES
  QT_ANDROID_PACKAGE_NAME "org.openrbx.client"
  QT_ANDROID_APP_NAME "OpenRBX"
  QT_ANDROID_MIN_SDK_VERSION 28
  QT_ANDROID_TARGET_SDK_VERSION 36
  QT_ANDROID_VERSION_NAME "0.1"
  QT_ANDROID_VERSION_CODE 1
  QT_ANDROID_PACKAGE_SOURCE_DIR "${OPENRBX_ANDROID_PACKAGE_DIR}"
  QT_ANDROID_EXTRA_LIBS "${OPENRBX_ANDROID_EXTRA_LIBS}"
)
