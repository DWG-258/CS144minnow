#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
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

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  debug( "unimplemented send_datagram called" );
  //first check ARP_table, if find, send to next_hop(TODO)
  if(ARP_table_.find(next_hop.ipv4_numeric())!=ARP_table_.end()){
    //send
    transmit( IPdatagram_transTo_EthernetFrame(dgram,next_hop.ipv4_numeric()) );
  }
  else{
    //send ARP request(dst address is broadcast)
    if(ARP_table_.find(next_hop.ipv4_numeric())==ARP_table_.end()){
      return;
    }
     EthernetFrame send_frame;
     send_frame.header.src=ethernet_address_;
     send_frame.header.dst=ETHERNET_BROADCAST;
     send_frame.header.type = EthernetHeader::TYPE_ARP;
     //TO DO ,purpuse is to find the MAC address of next_hop(use ARP)
     ARPMessage arp_request;
     //set arp_message(as payload)
     arp_request.sender_ethernet_address=ethernet_address_;
     arp_request.sender_ip_address=ip_address_.ipv4_numeric();
     arp_request.target_ip_address=next_hop.ipv4_numeric();
     arp_request.target_ethernet_address=ETHERNET_BROADCAST;
     arp_request.opcode=ARPMessage::OPCODE_REQUEST;
     //serialize arp_message to string stream(put into send_frame.payload)
      Serializer serializer;
      arp_request.serialize(serializer);
      send_frame.payload=serializer.finish();
     //cache datagram
     datagrams_cache_.push(dgram);
     //send arp_request
     transmit( send_frame );
     //start timer for 5s
     ARP_timer_[next_hop.ipv4_numeric()]=5000;
  }
 

}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  debug( "unimplemented recv_frame called" );
  if(frame.header.dst!=ethernet_address_&&frame.header.dst!=ETHERNET_BROADCAST)
  {
    return;
  }
  // ARP
  if(frame.header.type==EthernetHeader::TYPE_ARP){
    Parser parser(frame.payload);
    ARPMessage arp_message;
    arp_message.parse(parser);
    if(parser.has_error()) throw runtime_error("ARP_message parse error");

    //remember the mapping between the sender’s IP address and Ethernet address for 30 seconds
    //TODO,a timer?
    ARP_table_[arp_message.sender_ip_address]=arp_message.sender_ethernet_address;
    ARP_timer_[arp_message.sender_ip_address]=30000;

    if(arp_message.opcode==ARPMessage::OPCODE_REQUEST){
      //send ARP reply(dst address is sender address)
     EthernetFrame send_frame;
     send_frame.header.src=ethernet_address_;
     send_frame.header.dst=frame.header.src;
     send_frame.header.type = EthernetHeader::TYPE_ARP;
    
     ARPMessage arp_reply;
     //set arp_message(as payload)
     arp_reply.sender_ethernet_address=ethernet_address_;
     arp_reply.sender_ip_address=ip_address_.ipv4_numeric();
     arp_reply.target_ip_address=arp_message.sender_ip_address;
     arp_reply.target_ethernet_address=arp_message.sender_ethernet_address;
     arp_reply.opcode=ARPMessage::OPCODE_REPLY;
     //serialize arp_reply to string stream(put into send_frame.payload)
     Serializer serializer;
     arp_reply.serialize(serializer);
     send_frame.payload=serializer.finish();
     transmit( send_frame );
    }
    else if(arp_message.opcode==ARPMessage::OPCODE_REPLY){
      //send cached datagram
      if(!datagrams_cache_.empty()){
        InternetDatagram dgram=datagrams_cache_.front();
        datagrams_cache_.pop();
        transmit( IPdatagram_transTo_EthernetFrame(dgram,arp_message.sender_ip_address));
      }
    }
  
  }

  // IPv4
  if(frame.header.type==EthernetHeader::TYPE_IPv4){
    Parser parser(frame.payload);
    InternetDatagram ipv4_message;
    ipv4_message.parse(parser);
    if(parser.has_error()){
      throw runtime_error("ipv4_message parse error");
    }

    datagrams_received_.push(ipv4_message);
  }
  (void)frame;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick({}) called", ms_since_last_tick );
  //all done,wait for test
  for(auto it=ARP_timer_.begin();it!=ARP_timer_.end();it++)  {
    it->second-=ms_since_last_tick;
    if(it->second<=0){
      ARP_timer_.erase(it);
      ARP_table_.erase(it->first);
    }

  }

  for(auto it=ARP_timer_.begin();it!=ARP_timer_.end();it++) {
    it->second-=ms_since_last_tick;
    if(it->second<=0) {
      ARP_timer_.erase(it);
    }
  }
}

 EthernetFrame NetworkInterface::IPdatagram_transTo_EthernetFrame(const InternetDatagram& dgram,const uint32_t& next_hop)
 {
      EthernetFrame send_frame;
     send_frame.header.src=ethernet_address_;
     send_frame.header.dst = ARP_table_[next_hop];
     send_frame.header.type = EthernetHeader::TYPE_IPv4;
    //serialize dgram to string stream(put into send_frame.payload)
     Serializer serializer;
     dgram.serialize(serializer);
      send_frame.payload=serializer.finish();
      return send_frame;
 }