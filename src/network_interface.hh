#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <memory>
#include <queue>
#include <unordered_map>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack (connecting IP with the
// lower-layer network protocol, e.g. Ethernet). But the same module is also used
// repeatedly as part of a router: a router generally has many network interfaces,
// and the router's job is to route Internet datagrams between the different interfaces.

class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer) addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use ARP to look up the Ethernet destination address for the next hop.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  void recv_frame( EthernetFrame frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  static constexpr int ARP_MAPPING_TIMEOUT_MS = 30000; // ARP 映射有效期 30 秒
  static constexpr int ARP_REQUEST_TIMEOUT_MS = 5000;  // ARP 请求超时 5 秒

  // 把 IP 数据报封装为以太网帧（假设 next_hop 的 MAC 已知）
  EthernetFrame wrap_datagram( const InternetDatagram& dgram, const uint32_t& next_hop );

  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  std::string name_;
  std::shared_ptr<OutputPort> port_;
  EthernetAddress ethernet_address_;
  Address ip_address_;

  std::queue<InternetDatagram> datagrams_received_ {};

  // 等待 ARP 解析的数据报缓存（按下一跳 IP 分组）
  std::unordered_map<uint32_t, std::queue<InternetDatagram>> datagrams_cache_ {};

  // ARP 表（IP -> Ethernet）
  std::unordered_map<uint32_t, EthernetAddress> ARP_table_ {};

  // ARP 映射计时器（IP -> 剩余毫秒）
  std::unordered_map<uint32_t, int> ARP_timer_ {};

  // ARP 请求计时器（IP -> 剩余毫秒）
  std::unordered_map<uint32_t, int> ARP_request_timer_ {};
};
