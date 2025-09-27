#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";




  //TODO
  routing_table_.emplace(route_prefix,RouteTableEntry(route_prefix, prefix_length, next_hop, interface_num));
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  


  for( auto &interface_it : interfaces_ ) {
    // debug("[RoutingTable] Interface: {}\n", interface_it->name());
 
     while (!interface_it->datagrams_received().empty()){
     
      auto& dgram = interface_it->datagrams_received().front();
     if( dgram.header.ttl == 0 ) {
        interface_it->datagrams_received().pop();
        continue;
      }
     // decrement ttl every jump (sended)
      dgram.header.ttl--;
      if( dgram.header.ttl == 0 ) {
        interface_it->datagrams_received().pop();
        continue;
      }
      // after change TTL compue checksum
      dgram.header.compute_checksum();
      auto result = longest_prefix_match(dgram.header.dst);


      // if result.has_value() send it
      if(result.has_value()) {
        
        auto interface_send = interface(result.value().interface_num);
     
        if( result.value().next_hop.has_value() ) {
          interface_send->send_datagram(dgram, result.value().next_hop.value());
        }
        else {
          Address new_next_hop = Address::from_ipv4_numeric(dgram.header.dst);
          interface_send->send_datagram(dgram, new_next_hop);
   
        }
        
        interface_it->datagrams_received().pop();
      }
      else {
        //drop it
      interface_it->datagrams_received().pop();
      }

     }
    
  }
}

// Find the longest prefix match in the routing table ,for example 192.168.0.1/16 -> 192.168.0.0/16
 std::optional<RouteTableEntry> Router::longest_prefix_match(uint32_t ip_dst)
 {
   bitset<32> ip_dst_bitset(ip_dst);
   
   std::pair<std::optional<uint8_t>,RouteTableEntry> max_pair {std::nullopt,RouteTableEntry()};

   for(auto& route : routing_table_) {
    bitset<32> route_ip_bitset(route.second.route_prefix);
    uint8_t prefix_length = route.second.prefix_length;
    
    //if ip_dst matches route_ip
    if(route_ip_bitset.to_string().substr(0,prefix_length) == ip_dst_bitset.to_string().substr(0,prefix_length)){
      //find max prefix_length
      if(prefix_length >= max_pair.first.value_or(0)) {
       
            max_pair = std::make_pair(prefix_length,route.second);
      }
           
    }
    }
   

   if(max_pair.first.has_value()) 
    return max_pair.second;
   else
    return std::nullopt;
 }