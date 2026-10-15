#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>
#include <vector>

// A route table entry
struct RouteTableEntry
{
  uint32_t route_prefix {};
  uint8_t prefix_length {};
  std::optional<Address> next_hop {};
  size_t interface_num {};
};

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

  // longest prefix match
  std::optional<RouteTableEntry> longest_prefix_match( uint32_t ip_dst );

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  // The router's routing table（顺序存储，支持相同前缀、不同长度的多条路由）
  std::vector<RouteTableEntry> routing_table_ {};
};
