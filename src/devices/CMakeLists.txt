find_package(SDL2 REQUIRED)

noon_setup(noon_devices Noon.Devices
    SOURCES
      GamePadTranslator.hpp
    LINK
      Qt6::Core
      Qt6::Quick
      SDL2::SDL2
)
