#pragma once

#include "byte_stream.hh"
#include <map>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity.
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

  // 调试/速度测试用 accessor（兼容原测试）
  uint64_t get_byte() const { return next_index_; }
  uint64_t get_eof() const { return eof_index_; }

private:
  void merge_into_buffer( uint64_t first_index, std::string data );
  static std::string merge_overlap( uint64_t a_start,
                                    const std::string& a,
                                    uint64_t b_start,
                                    const std::string& b );

  ByteStream output_;
  std::map<uint64_t, std::string> buffer_ {}; // 待写入的片段，按索引有序

  uint64_t next_index_ { 0 }; // 下一个待写入输出流的绝对索引（= 已写入字节数）
  uint64_t eof_index_ { 0 };  // 流末尾的绝对索引（仅当 has_eof_ 时有意义）
  bool has_eof_ { false };
};
