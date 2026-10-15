#include "reassembler.hh"
#include "byte_stream.hh"

#include <algorithm>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  const uint64_t cap = output_.writer().available_capacity();
  const uint64_t first_unacceptable = next_index_ + cap; // 窗口上界（绝对流索引）

  // 记住 EOF（仅当明确知道流的终点时）
  if ( is_last_substring ) {
    has_eof_ = true;
    eof_index_ = first_index + data.size();
  }

  // 1) 丢弃已经写入输出流的部分
  if ( first_index < next_index_ ) {
    if ( first_index + data.size() <= next_index_ ) {
      return; // 整个片段都已写过
    }
    data = data.substr( next_index_ - first_index );
    first_index = next_index_;
  }

  // 2) 丢弃超出窗口的部分（即使前面的空洞被填上也无法写入）
  if ( first_index >= first_unacceptable ) {
    return;
  }
  if ( first_index + data.size() > first_unacceptable ) {
    data.resize( first_unacceptable - first_index );
  }

  // 3) 合并进有序片段集合
  merge_into_buffer( first_index, std::move( data ) );

  // 4) 把连续到 next_index_ 的前缀冲刷进输出流
  while ( !buffer_.empty() && buffer_.begin()->first == next_index_ ) {
    next_index_ += buffer_.begin()->second.size();
    output_.writer().push( std::move( buffer_.begin()->second ) );
    buffer_.erase( buffer_.begin() );
  }

  // 5) 已写入到 EOF，关闭流
  if ( has_eof_ && next_index_ == eof_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total = 0;
  for ( const auto& [idx, bytes] : buffer_ ) {
    total += bytes.size();
  }
  return total;
}

// 区间合并：把 [first_index, first_index + data.size()) 合并进 buffer_，消解所有重叠。
void Reassembler::merge_into_buffer( uint64_t first_index, std::string data )
{
  uint64_t new_start = first_index;
  uint64_t new_end = first_index + data.size();

  // 定位第一个可能与新片段重叠的旧片段（可能是 lower_bound 的前一个）
  auto it = buffer_.lower_bound( new_start );
  if ( it != buffer_.begin() ) {
    auto prev = std::prev( it );
    if ( prev->first + prev->second.size() >= new_start ) {
      it = prev; // 前一个片段伸进了新片段区间，从它开始合并
    }
  }

  // 向后吞并所有起点落在新片段区间内的旧片段
  while ( it != buffer_.end() && it->first <= new_end ) {
    data = merge_overlap( new_start, data, it->first, it->second );
    new_start = min( new_start, it->first );
    new_end = max( new_end, it->first + it->second.size() );
    it = buffer_.erase( it );
  }

  buffer_[new_start] = std::move( data );
}

// 两个重叠片段的字节级拼接。约定：返回 a、b 的并集。
string Reassembler::merge_overlap( uint64_t a_start,
                                   const string& a,
                                   uint64_t b_start,
                                   const string& b )
{
  // 保证 a 位于 b 的左侧（或相同起点）
  if ( a_start > b_start ) {
    return merge_overlap( b_start, b, a_start, a );
  }

  const uint64_t a_end = a_start + a.size();
  const uint64_t b_end = b_start + b.size();

  // b 完全被 a 覆盖：结果就是 a
  if ( a_end >= b_end ) {
    return a;
  }

  // a 在 b 左侧、部分重叠：a 的前缀 + b 未被覆盖的后缀
  return a.substr( 0, b_start - a_start ) + b;
}
