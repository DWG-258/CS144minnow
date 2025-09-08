#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  (void)data; // Your code here.
  if ( close_ ) {
    printf( "Writer is closed\n" );
    printf( "total_push_: %lu", total_push_ );
    return;
  }
  if ( current_size_ + data.size() > capacity_ ) {
    data = data.substr( 0, capacity_ - current_size_ );
  }
  for ( auto d : data ) {
    buffer.push_back( d );
  }
  current_size_ += data.size();
  total_push_ += data.size();
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

bool Writer::is_closed() const
{
  return close_;
  // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - current_size_; // Your code here.
}
uint64_t Writer::get_capacity() const
{
  return capacity_; // Your code here.
}
uint64_t Writer::bytes_pushed() const
{
  return total_push_; // Your code here.
}

string_view Reader::peek() const
{
  if ( buffer.empty() ) {
    printf( "Reader is empty\n" );
    return string_view();
  }
  string_view sv( &buffer.front(), 1 );
  return sv; // Your code here.
}

void Reader::pop( uint64_t len )
{
  (void)len; // Your code here.
  for ( uint64_t i = 0; i < len; i++ ) {
    buffer.pop_front();
    current_size_--;
    total_pop_++;
  }
}

bool Reader::is_finished() const
{
    debug( "is_finished: {},{}",close_,current_size_);
  if ( close_ && current_size_ == 0 )
    return true;
  else
    return false;
  // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return current_size_; // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return total_pop_; // Your code here.
}
