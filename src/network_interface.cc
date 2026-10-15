#include <iostream>
#include <utility>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();

  if ( ARP_table_.find( next_hop_ip ) != ARP_table_.end() ) {
    // 已知 MAC 地址，直接封装发送
    transmit( wrap_datagram( dgram, next_hop_ip ) );
    return;
  }

  // 缓存数据报，等待 ARP 解析
  datagrams_cache_[next_hop_ip].push( dgram );

  // 若已在等待该 IP 的 ARP 回复，不再重复发请求
  if ( ARP_request_timer_.find( next_hop_ip ) != ARP_request_timer_.end() ) {
    return;
  }

  // 构造并发送 ARP 请求
  ARPMessage arp_request;
  arp_request.sender_ethernet_address = ethernet_address_;
  arp_request.sender_ip_address = ip_address_.ipv4_numeric();
  arp_request.target_ip_address = next_hop_ip;
  arp_request.target_ethernet_address = ETHERNET_ZERO;
  arp_request.opcode = ARPMessage::OPCODE_REQUEST;

  EthernetFrame send_frame;
  send_frame.header.src = ethernet_address_;
  send_frame.header.dst = ETHERNET_BROADCAST;
  send_frame.header.type = EthernetHeader::TYPE_ARP;
  Serializer serializer;
  arp_request.serialize( serializer );
  send_frame.payload = serializer.finish();
  transmit( send_frame );

  ARP_request_timer_[next_hop_ip] = ARP_REQUEST_TIMEOUT_MS;
}

void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser parser( frame.payload );
    ARPMessage arp_message;
    arp_message.parse( parser );
    if ( parser.has_error() ) {
      throw runtime_error( "ARP_message parse error" );
    }

    // 学习 sender 的 IP -> MAC 映射，有效期 30 秒
    const uint32_t sender_ip = arp_message.sender_ip_address;
    ARP_table_[sender_ip] = arp_message.sender_ethernet_address;
    ARP_timer_[sender_ip] = ARP_MAPPING_TIMEOUT_MS;

    if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_message.target_ip_address != ip_address_.ipv4_numeric() ) {
        return; // 不是发给本机的请求
      }

      // 回复 ARP
      ARPMessage arp_reply;
      arp_reply.sender_ethernet_address = ethernet_address_;
      arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
      arp_reply.target_ip_address = sender_ip;
      arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
      arp_reply.opcode = ARPMessage::OPCODE_REPLY;

      EthernetFrame send_frame;
      send_frame.header.src = ethernet_address_;
      send_frame.header.dst = frame.header.src;
      send_frame.header.type = EthernetHeader::TYPE_ARP;
      Serializer serializer;
      arp_reply.serialize( serializer );
      send_frame.payload = serializer.finish();
      transmit( send_frame );
    } else if ( arp_message.opcode == ARPMessage::OPCODE_REPLY ) {
      // 收到回复：清理请求定时器，冲刷缓存的数据报
      ARP_request_timer_.erase( sender_ip );
      auto& queue = datagrams_cache_[sender_ip];
      while ( !queue.empty() ) {
        transmit( wrap_datagram( queue.front(), sender_ip ) );
        queue.pop();
      }
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser parser( frame.payload );
    InternetDatagram ipv4_message;
    ipv4_message.parse( parser );
    if ( parser.has_error() ) {
      return;
    }
    datagrams_received_.push( ipv4_message );
  }
}

void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 过期的 ARP 映射
  for ( auto it = ARP_timer_.begin(); it != ARP_timer_.end(); ) {
    it->second -= ms_since_last_tick;
    if ( it->second <= 0 ) {
      ARP_table_.erase( it->first );
      it = ARP_timer_.erase( it );
    } else {
      ++it;
    }
  }

  // 过期的 ARP 请求：丢弃该 IP 的全部缓存数据报，允许重新发起请求
  for ( auto it = ARP_request_timer_.begin(); it != ARP_request_timer_.end(); ) {
    it->second -= ms_since_last_tick;
    if ( it->second <= 0 ) {
      datagrams_cache_.erase( it->first );
      it = ARP_request_timer_.erase( it );
    } else {
      ++it;
    }
  }
}

EthernetFrame NetworkInterface::wrap_datagram( const InternetDatagram& dgram, const uint32_t& next_hop )
{
  auto it = ARP_table_.find( next_hop );
  if ( it == ARP_table_.end() ) {
    throw runtime_error( "ARP_table_ not found" );
  }

  EthernetFrame send_frame;
  send_frame.header.src = ethernet_address_;
  send_frame.header.dst = it->second;
  send_frame.header.type = EthernetHeader::TYPE_IPv4;
  Serializer serializer;
  dgram.serialize( serializer );
  send_frame.payload = serializer.finish();
  return send_frame;
}
