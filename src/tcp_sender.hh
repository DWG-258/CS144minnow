#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <deque>
#include <functional>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), rto_( initial_RTO_ms )
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
  uint64_t sequence_numbers_in_flight() const { return bytes_in_flight_; }
  uint64_t consecutive_retransmissions() const { return consecutive_retx_; }
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  // 未确认报文段：队头是最老（最早发送）的段
  struct Outstanding
  {
    uint64_t abs_seq; // 段首绝对序列号
    TCPSenderMessage msg;
    size_t length() const { return msg.sequence_length(); }
  };

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  uint64_t next_abs_seq_ { 0 }; // 下一个要发送的绝对序列号
  uint64_t bytes_in_flight_ { 0 }; // 在途（已发未确认）字节数
  uint64_t last_acked_ { 0 };      // 最近一次被新确认的绝对位置

  uint16_t window_size_ { 1 };
  bool syn_sent_ { false };
  bool fin_sent_ { false };
  bool error_ { false };

  uint64_t rto_;                // 当前重传超时
  uint64_t elapsed_ms_ { 0 };   // 自连接以来累计时间
  uint64_t abs_expire_ms_ { 0 }; // 下次超时的绝对时刻
  bool timer_running_ { false };
  uint64_t consecutive_retx_ { 0 };

  std::deque<Outstanding> outstanding_ {};
};
