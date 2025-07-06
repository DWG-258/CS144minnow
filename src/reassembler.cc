#include "reassembler.hh"
#include "debug.hh"
#include <iostream>
using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  uint64_t last_of_data=first_index+data.size();
  uint64_t capacity=writer().get_capacity();
  if(is_last_substring)
  {
      eof_index=first_index+data.size();
  }
  if(first_index<byte_writed_index)
  {
    //如果输出的index小于之前的，应该去掉重复部分
    // debug( "byte_writed_index={}",byte_writed_index);
    if(first_index+data.size()<byte_writed_index)
    {
      data="";
    }
    else
    {
      data = data.substr(byte_writed_index-first_index , writer().available_capacity());
      first_index=byte_writed_index;
      data=combine(first_index,data);
    }
    data = data.substr( 0, writer().available_capacity());
      output_.writer().push(data);
      byte_writed_index+=data.size();

 
  }
  else if(first_index==byte_writed_index)
  {
  
    //合并数据
    data=combine(first_index,data);
    //切割数据
    data = data.substr( 0, writer().available_capacity());

    output_.writer().push(data);
    byte_writed_index+=data.size();

  }
  else
  {
    //如果是后面的，则判断是否满足容量，不满足就丢弃，若满足还要判断重叠
    if(last_of_data>capacity)
    {
      //如果超出容量，则裁减
      uint64_t save_data_size=data.size()-last_of_data+capacity;
      data=data.substr(0,save_data_size);
     data = find_buffer_in_reassembler(first_index,data);
    }
    else
    {
      //如果没有超出，则开始判断重叠
      data = find_buffer_in_reassembler(first_index,data);
    }
  }
   //关闭流
  if(byte_writed_index==eof_index)
  {
    debug( "close stream" );
    debug( "byte_writed_index={}==eof_index {}",byte_writed_index,eof_index );

    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
 
  uint64_t total_bytes_pending=0;
  for(auto& i:buffer_in_reassembler)
  {
    total_bytes_pending+=i.second.size();
  }
  
  return total_bytes_pending;
}

std::string Reassembler::combine(uint64_t first_index,std::string& data)
{
  auto last_index=first_index+data.size();
  for(auto i=buffer_in_reassembler.begin();i!=buffer_in_reassembler.end();)
  {
    if((*i).first==last_index)
    {
      data+=(*i).second;
      last_index+=(*i).second.size();
      i=buffer_in_reassembler.erase(i);
    }
    else if((*i).first<last_index)
    {
      if((*i).first+(*i).second.size()>last_index)
      {
        data=data.substr(0,(*i).first-first_index)+(*i).second;
      last_index=first_index+data.size();
      i=buffer_in_reassembler.erase(i);
    }
      else
      {
        i=buffer_in_reassembler.erase(i);
      }
    }
    else if((*i).first>last_index)
    {
      break;
    }
  }
  return data;
}

// 查找buffer判断是否重叠,
std::string Reassembler::find_buffer_in_reassembler(uint64_t first_index,std::string& data)
{
  uint64_t last_of_data=first_index+data.size();
  bool include=false;

  for(auto i=buffer_in_reassembler.begin();i!=buffer_in_reassembler.end();)
  { 
    auto last_of_i=(*i).first+(*i).second.size();
     if((*i).first<=first_index&&last_of_i>=last_of_data)
    { 
      //包含重叠，无须处理
      include=true;
      return data; 
    }
    else if((*i).first>=last_of_data||last_of_i<=first_index)
    { 

      //不重叠，无须处理
      i++;
      continue;
    }
    else
    { 
      //有重叠buffer,处理重叠
      auto min_index=std::min(first_index,(*i).first);
      auto max_index=std::max(last_of_i,last_of_data);
      if(min_index==first_index&&max_index==last_of_data)
      {
      
        
        i=buffer_in_reassembler.erase(i);
        buffer_in_reassembler[min_index]=data;
      
        continue;
      }
      else if(min_index==first_index&&max_index==last_of_i)
      {
    
        data=data.substr(0,(*i).first-first_index)+(*i).second;
        i=buffer_in_reassembler.erase(i);
        buffer_in_reassembler[min_index]=data;
      
        continue;
       
      }
      else if(min_index==(*i).first&&max_index==last_of_data)
      {
       
        data=(*i).second.substr(0,first_index-(*i).first)+data;
        i=buffer_in_reassembler.erase(i); 
        buffer_in_reassembler[min_index]=data;
        first_index=min_index;
        
        continue;
      }
      else
      {
        return "";
      }
      
    }
  }
  if(!include)
  {
    buffer_in_reassembler[first_index]=data;
  }
  return data;


}