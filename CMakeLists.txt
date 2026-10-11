cmake_minimum_required(VERSION 3.21)

# The version lives here only: build_all.bat reads this line for the
# installer, bump_version.sh changes it.  Keep it on one line.
project(CRYPTARA VERSION 2.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)

if(MSVC)
  # The sources are UTF-8 (accents and emoji in the texts).
  add_compile_options(/utf-8 /W3)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

# ---------------------------------------------------------------- the core
# Everything but the QML window, shared by the program and the tests.
add_library(cryptara_core STATIC
  src/crypto/CryptoBackend.h
  src/crypto/CryptoManager.cpp src/crypto/CryptoManager.h
  src/crypto/DhGroup14.cpp
  src/crypto/DhPublicKey.cpp src/crypto/DhPublicKey.h
  src/app/Chime.cpp src/app/Chime.h
  src/app/Controller.cpp src/app/Controller.h
  src/app/I18n.cpp src/app/I18n.h
  src/app/Models.cpp src/app/Models.h
  src/app/Settings.cpp src/app/Settings.h
  src/vara/VaraClient.cpp src/vara/VaraClient.h
  src/vara/VaraProcess.cpp src/vara/VaraProcess.h
)
target_include_directories(cryptara_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(cryptara_core PUBLIC Qt6::Core Qt6::Network)

if(WIN32)
  # Windows CNG: part of the system, nothing to ship.
  target_sources(cryptara_core PRIVATE src/crypto/CryptoBackendCng.cpp)
  target_link_libraries(cryptara_core PUBLIC bcrypt winmm)
else()
  # Development builds on Linux only.
  find_package(OpenSSL 3 REQUIRED)
  target_sources(cryptara_core PRIVATE src/crypto/CryptoBackendOpenSsl.cpp)
  target_link_libraries(cryptara_core PUBLIC OpenSSL::Crypto)
endif()

# ------------------------------------------------------------- the program
qt_add_executable(cryptara WIN32 src/main.cpp src/UiHelper.h src/resources.qrc)
set_target_properties(cryptara PROPERTIES OUTPUT_NAME CRYPTARA)
target_link_libraries(cryptara PRIVATE cryptara_core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(WIN32)
  # Icon and version information (name, publisher, version): an executable
  # that carries them is better treated by SmartScreen and antivirus tools.
  target_sources(cryptara PRIVATE assets/cryptara.rc)
  target_include_directories(cryptara PRIVATE assets ${CMAKE_BINARY_DIR}/generated)
endif()

# ------------------------------------------------------------------- tests
include(CTest)
if(BUILD_TESTING)
  add_executable(test_crypto tests/test_crypto.cpp)
  target_include_directories(test_crypto PRIVATE tests)
  target_link_libraries(test_crypto PRIVATE cryptara_core)
  add_test(NAME crypto COMMAND test_crypto)

  add_executable(test_core tests/test_core.cpp tests/FakeVara.h)
  target_link_libraries(test_core PRIVATE cryptara_core)
  add_test(NAME core COMMAND test_core)
  target_compile_definitions(test_core PRIVATE CRYPTARA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

  # The engine on its own, driven over stdin/stdout: tests/interop runs it
  # against the Python version through a simulated VARA.
  add_executable(cryptara_headless tests/headless.cpp)
  target_link_libraries(cryptara_headless PRIVATE cryptara_core)
endif()
