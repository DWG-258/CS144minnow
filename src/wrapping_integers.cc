#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( zero_point.raw_value_ + n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 相对零点的偏移（0 ~ 2^32 - 1，uint32 自动回绕）
  const uint64_t diff = static_cast<uint32_t>( raw_value_ - zero_point.raw_value_ );

  // 以 checkpoint 向下取整到 2^32 的倍数为基准，得到同周期候选
  const uint64_t base = checkpoint & ~0xFFFFFFFFull;
  const uint64_t cand = base + diff;

  // 上、下各偏移一个周期，得到另外两个候选
  const uint64_t cand_hi = cand + ( 1ull << 32 );
  const uint64_t cand_lo = ( cand >= ( 1ull << 32 ) ) ? ( cand - ( 1ull << 32 ) ) : cand;

  // 取三者中离 checkpoint 最近的那个
  auto dist = [&]( uint64_t x ) { return ( x >= checkpoint ) ? ( x - checkpoint ) : ( checkpoint - x ); };

  uint64_t best = cand;
  if ( dist( cand_hi ) < dist( best ) ) {
    best = cand_hi;
  }
  if ( dist( cand_lo ) < dist( best ) ) {
    best = cand_lo;
  }
  return best;
}
