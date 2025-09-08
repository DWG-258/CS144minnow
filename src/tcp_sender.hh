#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <map>

#include <functional>



class RetransTimer
{
private:
  bool is_running_ {false};
  uint64_t RTO_ms_;
public:
  RetransTimer(uint64_t RTO_ms):RTO_ms_(RTO_ms){}
  void start(){
    is_running_ = true;
  }
  void stop(){
    is_running_ = false;
  }
  bool is_running(){
    return is_running_;
  }
 
  void set_RTO(uint64_t RTO_ms){
    RTO_ms_ = RTO_ms;
  }
  uint64_t get_RTO()
  {
    return RTO_ms_;
  }
  void reduce_time(uint64_t ms)
  {
    if(RTO_ms_ > ms)
    RTO_ms_ -= ms;
    else RTO_ms_ = 0;
  }
};

  struct msg_with_timer
  {
    TCPSenderMessage msg;
    RetransTimer timer;
  };
class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

  void restart_timer(uint64_t ms) {
 
    retrans_timer_.set_RTO(ms);
    retrans_timer_.start();
  }
private:
  Reader& reader() { return input_.reader(); }

  
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  RetransTimer retrans_timer_{initial_RTO_ms_};
  uint64_t RTO_ms_ {initial_RTO_ms_};
  // msg_with_timer msg_with_timer_{{},retrans_timer_};
  TCPSenderMessage first_unack_msg_{};
  uint16_t window_size_  {1};
  std::map<uint64_t,TCPSenderMessage> outstanding_sequences_{}; 
  std::map<uint64_t,TCPSenderMessage> sender_sequences_{};  
  uint64_t window_left_edge_ {0};
  uint64_t old_max_seq_ {0};
  uint64_t stream_seq_ {0};
  uint64_t lived_time_ {0};
  uint64_t consecutive_retransmissions_count_ {0};
  bool is_fin_sent_ {false};
  uint64_t sequence_size_aftercut_ {0}; 
};
