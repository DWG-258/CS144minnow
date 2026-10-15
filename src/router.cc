#include "router.hh"

#include <iostream>

using namespace std;

void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  routing_table_.push_back( RouteTableEntry { route_prefix, prefix_length, next_hop, interface_num } );
}

void Router::route()
{
  for ( auto& interface_it : interfaces_ ) {
    while ( !interface_it->datagrams_received().empty() ) {
      auto dgram = interface_it->datagrams_received().front();
      interface_it->datagrams_received().pop();

      // TTL 减 1 后若为 0，则丢弃
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      dgram.header.ttl--;
      dgram.header.compute_checksum();

      auto result = longest_prefix_match( dgram.header.dst );
      if ( !result.has_value() ) {
        continue; // 无匹配路由，丢弃
      }

      auto interface_send = interface( result->interface_num );
      const Address next_hop = result->next_hop.has_value()
                                 ? result->next_hop.value()
                                 : Address::from_ipv4_numeric( dgram.header.dst );
      interface_send->send_datagram( dgram, next_hop );
    }
  }
}

// 位运算实现的最长前缀匹配：用掩码比较高 prefix_length 位，无需字符串转换。
std::optional<RouteTableEntry> Router::longest_prefix_match( uint32_t ip_dst )
{
  std::optional<RouteTableEntry> best;
  uint8_t best_len = 0;

  for ( const auto& r : routing_table_ ) {
    if ( r.prefix_length < best_len ) {
      continue; // 已找到更长的匹配，跳过更短的
    }
    // prefix_length == 0 时掩码为 0（匹配所有地址）
    const uint32_t mask = ( r.prefix_length == 0 ) ? 0 : ( ~0u << ( 32 - r.prefix_length ) );
    if ( ( ip_dst & mask ) == ( r.route_prefix & mask ) ) {
      best = r;
      best_len = r.prefix_length;
    }
  }

  return best;
}
