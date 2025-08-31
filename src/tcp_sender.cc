#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  debug( "unimplemented sequence_numbers_in_flight() called" );
  return {};
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  debug( "unimplemented consecutive_retransmissions() called" );
  return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  debug( "unimplemented push() called" );
  TCPSenderMessage msg = make_empty_message();
  if(reader().is_finished()){
    msg.FIN=true;
  }
    uint64_t send_size=window_size_;
  if(send_size>TCPConfig::MAX_PAYLOAD_SIZE){
    send_size=TCPConfig::MAX_PAYLOAD_SIZE;
  }
  read(reader(),send_size,msg.payload);

  uint64_t seq=reader().bytes_popped();
  if(seq==0){
    msg.SYN=true;
  }
  msg.seqno=msg.seqno.wrap( seq, msg.seqno );
  

  transmit( msg );

}

TCPSenderMessage TCPSender::make_empty_message() const
{
  debug( "unimplemented make_empty_message() called" );
  TCPSenderMessage msg;
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  debug( "unimplemented receive() called" );
  window_size_=msg.window_size;
  if(msg.ackno.has_value())
  { 
  }
  (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
}
