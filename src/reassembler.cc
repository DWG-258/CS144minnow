#include "reassembler.hh"
#include "byte_stream.hh"
#include "debug.hh"
#include <iostream>
using namespace std;

// 主要函数
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{

  // 该数据的最后索引
  uint64_t last_index_data = first_index + data.size();
  // 流的容量
  uint64_t capacity = writer().get_capacity();
  if ( is_last_substring ) {
    eof_index = first_index + data.size();
  }
  if ( first_index < byte_writed_index ) {
    // 如果输出的index小于之前的，应该去掉重复部分
    if ( last_index_data < byte_writed_index ) {
      data.clear();
    } else {
      data = data.substr( byte_writed_index - first_index, writer().available_capacity() );
      first_index = byte_writed_index;
    }
  }

  if ( first_index == byte_writed_index ) {

    // index刚好等于
    data = check_reassembler( first_index, data, false );
    data = data.substr( 0, std::min( data.size(),  writer().available_capacity() ) );
    output_.writer().push( data );

    byte_writed_index += data.size();
  } else {
    // 如果是后面的，则判断是否满足容量，不满足就丢弃，若满足还要在判断重叠
    //逻辑错误修改，capacity是bytestream窗口的大小，eof_index才是data最后的大小（需要多个window大小）
    if(first_index<=eof_index){
      data = data.substr( 0, std::min( data.size(),capacity-first_index));
      check_reassembler( first_index, data, true );
    }
  }
  if ( is_last_substring ) {
    eof_index = std::max(eof_index,first_index + data.size());
  }

  // 关闭流
  if ( byte_writed_index == eof_index ) {
    output_.writer().close();
  }

}
// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total_bytes_pending = 0;
  for ( auto& i : buffer_in_reassembler ) {
    total_bytes_pending += i.second.size();
  }

  return total_bytes_pending;
}
std::string overlap( uint64_t first_d1, string& d1, uint64_t first_d2, string& d2 )
{
  uint64_t r1 = first_d1 + d1.size();
  uint64_t r2 = first_d2 + d2.size();
    if(r1==first_d2)
    {
      return d1+d2;
    }

    if ( first_d1 < first_d2 && r1 <= r2 ) {
      return d1.substr( 0, first_d2- first_d1 ) + d2;
    }
    if ( first_d1 >= first_d2 && r1 <= r2 ) {
      return d2;
    }
    if ( first_d1 < first_d2 && r1 > r2 ) {
      return d1;
    }
   
    if ( first_d1 >= first_d2 && r1 > r2 ) {
      return d2 + d1.substr( r2 - first_d1 );
    }
       if(r2==first_d1)
    {
      return d2+d1;
    }
  
  return d1;
}

std::string Reassembler::check_reassembler( uint64_t first_index, std::string& data, bool isCache )
{
  if(data.empty()) return data;
  uint64_t last_index = first_index + data.size();
  auto it = buffer_in_reassembler.lower_bound( first_index );
  // 检查前一个区间
  if ( it != buffer_in_reassembler.begin() ) {
    auto prev = std::prev( it );
    if ( prev->first + prev->second.size() >= first_index ) {
      it = prev; // 有重叠，往前合并
    }
  }
  uint64_t newF = first_index, newL = last_index;
  std::string temp_data = data;
  // 合并所有重叠区间
  while ( it != buffer_in_reassembler.end() && it->first <= last_index ) {
    
    newL = std::max( newL, it->first + it->second.size() );
    temp_data = overlap( newF, temp_data, it->first, it->second );
    newF = std::min( newF, it->first );
    it = buffer_in_reassembler.erase( it ); // 删除旧区间
  }
  if ( isCache ) {
    // 插入合并后的新区间
    buffer_in_reassembler[newF] = temp_data;
  }
  
  return temp_data;
}
