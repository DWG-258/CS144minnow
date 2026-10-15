#include "byte_stream.hh"

#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( closed_ ) {
    return;
  }

  // 只写入可用容量内的部分，一次性截断
  const uint64_t avail = capacity_ - buffer_.size();
  if ( data.size() > avail ) {
    data.resize( avail );
  }

  buffer_ += data; // 批量 append，替代逐字节 push_back
  total_pushed_ += data.size();
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::get_capacity() const
{
  return capacity_;
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

string_view Reader::peek() const
{
  return string_view( buffer_ ); // 返回整个可用区间，而非单字节
}

void Reader::pop( uint64_t len )
{
  len = min( len, static_cast<uint64_t>( buffer_.size() ) );
  buffer_.erase( 0, len ); // 一次性删除，替代逐字节 pop_front
  total_popped_ += len;
}

bool Reader::is_finished() const
{
  return closed_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}
