#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{

  uint64_t count = 0;
  auto it = outstanding_sequences_.begin();
  if ( sequence_size_aftercut_ > 0 )
    ++it;
  for ( auto i = it; i != outstanding_sequences_.end(); ++i )
    count += i->second.sequence_length();

  return count + sequence_size_aftercut_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{

  return consecutive_retransmissions_count_;
}

void TCPSender::push( const TransmitFunction& transmit )
{

  // create a empty msg and set its property
  while ( true ) {
    TCPSenderMessage msg {};

    if ( input_.has_error() )
      msg.RST = true;

    uint64_t send_size = max( (uint16_t)1, window_size_ ) - sequence_numbers_in_flight();

    if ( stream_seq_ == 0 && send_size > 0 ) {
      msg.SYN = true;
      msg.seqno = isn_;
    }

    uint64_t sender_know_window_size = send_size;
    if ( send_size > TCPConfig::MAX_PAYLOAD_SIZE ) {
      send_size = TCPConfig::MAX_PAYLOAD_SIZE;
    }
    auto payload_len = send_size;
    if ( msg.SYN )
      payload_len = send_size == 0 ? 0 : send_size - 1;
    read( reader(), payload_len, msg.payload );

    bool is_window_full = sender_know_window_size == msg.payload.size() || send_size == 0;
    if ( is_window_full && send_size == 0 )
      break;

    if ( reader().is_finished() && !is_window_full && !is_fin_sent_ ) {
      msg.FIN = true;
      is_fin_sent_ = true;
    }
    // stream_seq_=reader().bytes_popped();

    msg.seqno = msg.seqno.wrap( stream_seq_, isn_ );

    // start timer
    if ( msg.sequence_length() > 0 && !retrans_timer_.is_running() ) {
      retrans_timer_.start();
    }

    if ( msg.sequence_length() > 0 ) {

      auto msg_length = msg.sequence_length();
      // trans the msg
      transmit( msg );
      // store the not acknoed sequence
      outstanding_sequences_.emplace( stream_seq_, std::move( msg ) );
      first_unack_msg_ = outstanding_sequences_.begin()->second;
      stream_seq_ += msg_length;
    }

    if ( is_fin_sent_ || window_size_ - sequence_numbers_in_flight() == 0 || reader().bytes_buffered() == 0 )
      break;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{

  // return TCPSenderMessage (seqno is right but message is empty);
  TCPSenderMessage msg {};
  if ( input_.has_error() )
    msg.RST = true;

  msg.seqno = msg.seqno.wrap( stream_seq_, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{

  window_size_ = msg.window_size;

  uint64_t revice_left_window = 0;
  // update outstanding_sequences_
  if ( msg.ackno.has_value() ) {
    revice_left_window = msg.ackno.value().unwrap( isn_, stream_seq_ );
  } else if ( window_size_ == 0 )
    input_.set_error();
  // remove the acknoed sequence

  if ( revice_left_window > stream_seq_ || revice_left_window <= old_max_seq_ )
    return;
  window_left_edge_ = revice_left_window;

  auto it = outstanding_sequences_.lower_bound( window_left_edge_ );
  auto prev_it = it == outstanding_sequences_.begin() ? it : prev( it );
  uint64_t back_seq = prev_it->first + prev_it->second.sequence_length();

  bool is_cut = false;
  if ( back_seq > window_left_edge_ ) {
    sequence_size_aftercut_ = back_seq - window_left_edge_;

    is_cut = true;
    it = prev_it;
  }
  for ( auto i = outstanding_sequences_.begin(); i != it; ) {
    i = outstanding_sequences_.erase( i );
  }

  if ( !is_cut ) {
    sequence_size_aftercut_ = 0;
  }

  if ( window_left_edge_ > old_max_seq_ ) {
    old_max_seq_ = window_left_edge_;
    // reset RTO_MS
    RTO_ms_ = initial_RTO_ms_;
    // restart timer
    restart_timer( RTO_ms_ );
    // reset consecutive_retransmissions_count
    consecutive_retransmissions_count_ = 0;
  }

  // all are acknoed, stop timer
  if ( outstanding_sequences_.empty() )
    retrans_timer_.stop();
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // the timer is always for the first unacknoed sequence
  // timer running
  if ( retrans_timer_.is_running() ) {

    retrans_timer_.reduce_time( ms_since_last_tick );
    // retrans_timer_.reduce_time(ms_since_last_tick);
    // timer stop

    if ( retrans_timer_.get_RTO() == 0 ) {
      retrans_timer_.stop();
      // retransmit the first unacknoed sequence

      transmit( outstanding_sequences_.begin()->second );
      if ( window_size_ > 0 ) {
        consecutive_retransmissions_count_++;
        RTO_ms_ = RTO_ms_ * 2;
      }
      // everytime retransmit,restart timer
      restart_timer( RTO_ms_ );
    }
  }

  (void)transmit;
}
