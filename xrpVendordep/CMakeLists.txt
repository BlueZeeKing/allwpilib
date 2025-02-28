project(xrpVendordep)

include(SubDirList)
include(CompileWarnings)
include(AddTest)

if (WITH_JAVA)
  find_package(Java REQUIRED)
  include(UseJava)
  set(CMAKE_JAVA_COMPILE_FLAGS "-encoding" "UTF8" "-Xlint:unchecked")

  file(GLOB_RECURSE JAVA_SOURCES src/main/java/*.java)
  add_jar(xrpVendordep_jar ${JAVA_SOURCES} INCLUDE_JARS hal_jar ntcore_jar cscore_jar cameraserver_jar wpimath_jar wpiutil_jar wpilibj_jar OUTPUT_NAME xrpVendordep)

  get_property(xrpVendordep_JAR_FILE TARGET xrpVendordep_jar PROPERTY JAR_FILE)
  install(FILES ${xrpVendordep_JAR_FILE} DESTINATION "${java_lib_dest}")

  set_property(TARGET xrpVendordep_jar PROPERTY FOLDER "java")

  if (WITH_FLAT_INSTALL)
      set (xrpVendordep_config_dir ${wpilib_dest})
  else()
      set (xrpVendordep_config_dir share/xrpVendordep)
  endif()
endif()

file(GLOB_RECURSE xrpVendordep_native_src src/main/native/cpp/*.cpp)
add_library(xrpVendordep ${xrpVendordep_native_src})
set_target_properties(xrpVendordep PROPERTIES DEBUG_POSTFIX "d")
set_property(TARGET xrpVendordep PROPERTY FOLDER "libraries")

target_compile_features(xrpVendordep PUBLIC cxx_std_20)
wpilib_target_warnings(xrpVendordep)
target_link_libraries(xrpVendordep wpilibc)

target_include_directories(xrpVendordep PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/main/native/include>
                            $<INSTALL_INTERFACE:${include_dest}/xrpVendordep>)

install(TARGETS xrpVendordep EXPORT xrpVendordep DESTINATION "${main_lib_dest}")
install(DIRECTORY src/main/native/include/ DESTINATION "${include_dest}/xrpVendordep")

if (FLAT_INSTALL_WPILIB)
     set(xrpVendordep_config_dir ${wpilib_dest})
 else()
     set(xrpVendordep_config_dir share/xrpVendordep)
 endif()

 configure_file(xrpVendordep-config.cmake.in ${WPILIB_BINARY_DIR}/xrpVendordep-config.cmake)
 install(FILES ${WPILIB_BINARY_DIR}/xrpVendordep-config.cmake DESTINATION ${xrpVendordep_config_dir})
 install(EXPORT xrpVendordep DESTINATION ${xrpVendordep_config_dir})

 if (WITH_TESTS)
     wpilib_add_test(xrpVendordep src/test/native/cpp)
     target_include_directories(xrpVendordep_test PRIVATE src/test/native/include)
     target_link_libraries(xrpVendordep_test xrpVendordep gmock_main)
 endif()
