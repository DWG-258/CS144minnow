#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  uint32_t res = zero_point.raw_value_ + n;
  //   debug("____");
  // debug( "wrap( value={}, {} ) called", n, zero_point.raw_value_ );
  return Wrap32 { res };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{

  // Your code here.
  // uint32_t会自动回绕
  uint64_t diff = raw_value_ - zero_point.raw_value_;
  uint64_t n = checkpoint / ( 1ull << 32 );
  uint64_t modN = checkpoint % ( 1ull << 32 );
  ;
  if ( modN > diff && modN - diff > ( 1ull << 31 ) ) {
    n++;
  }
  if ( modN < diff && diff - modN > ( 1ull << 31 ) ) {
    n = ( n == 0 ) ? 0 : n - 1;
  }
  diff += n * ( 1ull << 32 );

  // debug( "unwrap( {}, {} ,{}) called", raw_value_, checkpoint ,zero_point.raw_value_);

  return  diff ;
}
