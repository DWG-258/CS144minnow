#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  checkpoint_ = reassembler_.writer().bytes_pushed();
  if ( message.RST ) {
    reassembler_.byteStream().set_error();
    return;
  }
  if ( message.SYN ) {
    start_stream_ = message.seqno;
    syn_received_ = true;
  }
  if ( message.FIN )
    fin_received_ = true;
  first_index_in_TCP = message.seqno.unwrap( start_stream_, checkpoint_ );
  bool is_vaild_substr = true;
  if ( first_index_in_TCP == 0 && !message.SYN )
    is_vaild_substr = false;
  if ( first_index_in_TCP != 0 )
    first_index_in_TCP--;
  if ( is_vaild_substr && syn_received_ )
    reassembler_.insert( first_index_in_TCP, message.payload, message.FIN );

  auto now_index_in_reassembler = reassembler_.writer().bytes_pushed();
  if ( fin_received_ && ( now_index_in_reassembler == reassembler_.get_eof() ) )
    str_finished_ = true;
  if ( syn_received_ ) {
    next_seq_no_ = message.seqno.wrap( now_index_in_reassembler + 1, start_stream_ );
    if ( str_finished_ )
      *next_seq_no_ = *next_seq_no_ + 1;
  }
  (void)message;
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage ret;
  ret.ackno = next_seq_no_;
  uint64_t capacity = reassembler_.writer().get_capacity();
  uint64_t available_capacity = reassembler_.writer().available_capacity();
  if ( capacity >= UINT16_MAX )
    capacity = UINT16_MAX;
  if ( available_capacity <= capacity ) {
    ret.window_size = available_capacity;
  } else
    ret.window_size = capacity;
  ret.RST = false;
  if ( reassembler_.byteStream().has_error() )
    ret.RST = true;
  return ret;
}
