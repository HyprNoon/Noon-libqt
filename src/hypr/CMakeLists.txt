find_package(Qt6 REQUIRED COMPONENTS Network)

noon_setup(noon_hypr Noon.Hypr
    SOURCES
    HyprBridge.hpp
    HyprParser.hpp
    ScreenTimeManager.hpp
    ScreenTimeManager.cpp
    LINK
      Qt6::Core
      Qt6::Network
      Qt6::Concurrent
      Qt6::Quick
)
