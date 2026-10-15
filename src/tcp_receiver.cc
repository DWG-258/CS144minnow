#include "tcp_receiver.hh"

#include <algorithm>
#include <utility>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    error_ = true;
    return;
  }

  // 记录 ISN（SYN 的序列号）
  if ( message.SYN ) {
    isn_ = message.seqno;
    syn_received_ = true;
  }

  if ( !syn_received_ ) {
    return; // 未收到 SYN 前丢弃所有数据
  }

  // seqno -> 绝对序列号 -> 流索引
  // payload 起始的绝对序列号 = unwrap(seqno)；去掉 SYN 占用的 1 个序列号即为流索引
  const uint64_t abs_seq = message.seqno.unwrap( isn_, checkpoint_ );
  const uint64_t stream_idx = abs_seq - 1; // SYN 占 1 个序列号

  reassembler_.insert( stream_idx, std::move( message.payload ), message.FIN );

  // 用「已重组字节数」更新 checkpoint（单调递增，是稳定的参考点）
  checkpoint_ = reassembler_.reader().bytes_popped();
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  msg.window_size = static_cast<uint16_t>(
    min( reassembler_.writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  msg.RST = error_;

  if ( syn_received_ ) {
    // ackno = 已重组字节数 + 1（SYN）+（流已结束时再加 1 给 FIN）
    const uint64_t abs_ack = reassembler_.reader().bytes_popped() + 1
                             + ( reassembler_.reader().is_finished() ? 1 : 0 );
    msg.ackno = Wrap32::wrap( abs_ack, isn_ );
  }

  return msg;
}
