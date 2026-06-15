noon_setup(noon_devices Noon.Devices
    SOURCES
      GamePadTranslator.hpp
    QT_COMPONENTS
      Core Quick
    PACKAGES
      SDL2
    LINK
      Qt6::Core
      Qt6::Quick
      SDL2::SDL2
)
