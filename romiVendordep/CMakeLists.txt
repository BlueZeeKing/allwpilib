project(romiVendordep)

include(SubDirList)
include(CompileWarnings)
include(AddTest)

if (WITH_JAVA)
  find_package(Java REQUIRED)
  include(UseJava)
  set(CMAKE_JAVA_COMPILE_FLAGS "-encoding" "UTF8" "-Xlint:unchecked")

  file(GLOB_RECURSE JAVA_SOURCES src/main/java/*.java)
  add_jar(romiVendordep_jar ${JAVA_SOURCES} INCLUDE_JARS hal_jar ntcore_jar cscore_jar cameraserver_jar wpimath_jar wpiutil_jar wpilibj_jar OUTPUT_NAME romiVendordep)

  get_property(ROMIVENDORDEP_JAR_FILE TARGET romiVendordep_jar PROPERTY JAR_FILE)
  install(FILES ${ROMIVENDORDEP_JAR_FILE} DESTINATION "${java_lib_dest}")

  set_property(TARGET romiVendordep_jar PROPERTY FOLDER "java")

  if (WITH_FLAT_INSTALL)
      set (romiVendordep_config_dir ${wpilib_dest})
  else()
      set (romiVendordep_config_dir share/romiVendordep)
  endif()
endif()

file(GLOB_RECURSE romiVendordep_native_src src/main/native/cpp/*.cpp)
add_library(romiVendordep ${romiVendordep_native_src})
set_target_properties(romiVendordep PROPERTIES DEBUG_POSTFIX "d")
set_property(TARGET romiVendordep PROPERTY FOLDER "libraries")

target_compile_features(romiVendordep PUBLIC cxx_std_20)
wpilib_target_warnings(romiVendordep)
target_link_libraries(romiVendordep wpilibc)

target_include_directories(romiVendordep PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/main/native/include>
                            $<INSTALL_INTERFACE:${include_dest}/romiVendordep>)

install(TARGETS romiVendordep EXPORT romiVendordep DESTINATION "${main_lib_dest}")
install(DIRECTORY src/main/native/include/ DESTINATION "${include_dest}/romiVendordep")

if (FLAT_INSTALL_WPILIB)
     set(romiVendordep_config_dir ${wpilib_dest})
 else()
     set(romiVendordep_config_dir share/romiVendordep)
 endif()

 configure_file(romiVendordep-config.cmake.in ${WPILIB_BINARY_DIR}/romiVendordep-config.cmake)
 install(FILES ${WPILIB_BINARY_DIR}/romiVendordep-config.cmake DESTINATION ${romiVendordep_config_dir})
 install(EXPORT romiVendordep DESTINATION ${romiVendordep_config_dir})

 if (WITH_TESTS)
     wpilib_add_test(romiVendordep src/test/native/cpp)
     target_include_directories(romiVendordep_test PRIVATE src/test/native/include)
     target_link_libraries(romiVendordep_test romiVendordep gmock_main)
 endif()
