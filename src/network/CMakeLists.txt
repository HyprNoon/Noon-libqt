noon_setup(noon_network Noon.Network
    SOURCES
      NetworkManager.hpp
      NetworkManager.cpp
    QT_COMPONENTS
      Core Network Concurrent Quick
    PACKAGES
      KF6NetworkManagerQt
    LINK
      KF6::NetworkManagerQt
      Qt6::Core
      Qt6::Network
      Qt6::Concurrent
      Qt6::Quick
)
