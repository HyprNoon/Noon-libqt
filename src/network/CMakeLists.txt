find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(KF6NetworkManagerQt REQUIRED)

noon_setup(noon_network Noon.Network
    SOURCES
      NetworkManager.hpp
      NetworkManager.cpp
    LINK
      KF6::NetworkManagerQt
      Qt6::Core
      Qt6::Network
      Qt6::Concurrent
      Qt6::Quick
)
