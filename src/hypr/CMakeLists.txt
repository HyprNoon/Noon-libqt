noon_setup(noon_hypr Noon.Hypr
    SOURCES
    HyprBridge.hpp
    HyprParser.hpp
    ScreenTimeManager.hpp
    ScreenTimeManager.cpp
    QT_COMPONENTS
      Core Network Concurrent Quick
    LINK
      Qt6::Core
      Qt6::Network
      Qt6::Concurrent
      Qt6::Quick
)
