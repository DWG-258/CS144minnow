#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retx_;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_abs_seq_, isn_ );
  if ( error_ || input_.has_error() ) {
    msg.RST = true;
  }
  return msg;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // 流出错时：只发送一个带 RST 的空段（不带 payload），然后不再发送新数据
  if ( input_.has_error() ) {
    TCPSenderMessage msg;
    msg.RST = true;
    msg.seqno = Wrap32::wrap( next_abs_seq_, isn_ );
    transmit( msg );
    error_ = true;
    return;
  }
  if ( error_ ) {
    return;
  }

  // 只要窗口还有空位，就持续发送
  while ( bytes_in_flight_ < window_size_ ) {
    TCPSenderMessage msg;

    if ( !syn_sent_ ) {
      msg.SYN = true;
      syn_sent_ = true;
    }

    // 还能装入多少 payload（扣除 SYN/FIN 占用的序列号）
    const uint64_t remaining = window_size_ - bytes_in_flight_ - msg.sequence_length();
    const size_t payload_len = min( remaining, static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ) );
    read( reader(), payload_len, msg.payload );

    // FIN：流已结束、且 FIN 连同 payload 能塞进窗口时追加
    if ( !fin_sent_ && reader().is_finished()
         && msg.sequence_length() + 1 <= window_size_ - bytes_in_flight_ ) {
      msg.FIN = true;
      fin_sent_ = true;
    }

    if ( msg.sequence_length() == 0 ) {
      break; // 没有新字节可发
    }

    const uint64_t abs_seq = next_abs_seq_; // 段首绝对序列号
    msg.seqno = Wrap32::wrap( abs_seq, isn_ );
    next_abs_seq_ += msg.sequence_length();
    bytes_in_flight_ += msg.sequence_length();

    outstanding_.push_back( Outstanding { abs_seq, msg } );
    transmit( msg );

    if ( !timer_running_ ) {
      timer_running_ = true;
      abs_expire_ms_ = elapsed_ms_ + rto_;
    }
  }
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    error_ = true;
    return;
  }

  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return; // 无 ackno（如 SYN 前的空包），忽略
  }

  const uint64_t ack_abs = msg.ackno->unwrap( isn_, next_abs_seq_ );
  if ( ack_abs > next_abs_seq_ ) {
    return; // 确认了尚未发送的数据，非法 ACK，忽略
  }

  // 累积确认：弹出所有「段尾 <= ack」的完整段
  while ( !outstanding_.empty()
          && outstanding_.front().abs_seq + outstanding_.front().length() <= ack_abs ) {
    bytes_in_flight_ -= outstanding_.front().length();
    outstanding_.pop_front();
  }

  // 有新数据被确认 → 复位 RTO 与重传计数
  if ( ack_abs > last_acked_ ) {
    last_acked_ = ack_abs;
    rto_ = initial_RTO_ms_;
    consecutive_retx_ = 0;
    if ( outstanding_.empty() ) {
      timer_running_ = false; // 全部确认，停止定时器
    } else {
      abs_expire_ms_ = elapsed_ms_ + rto_; // 仍有在途，重设到期时刻
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  elapsed_ms_ += ms_since_last_tick;

  if ( timer_running_ && elapsed_ms_ >= abs_expire_ms_ ) {
    // 重传队头（最老未确认段）
    if ( !outstanding_.empty() ) {
      transmit( outstanding_.front().msg );
    }

    // 指数退避：仅当窗口非零时增长（零窗口不是拥塞）
    if ( window_size_ > 0 ) {
      ++consecutive_retx_;
      rto_ *= 2;
    }
    abs_expire_ms_ = elapsed_ms_ + rto_;
  }
}
