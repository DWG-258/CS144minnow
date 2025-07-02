#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <unistd.h>
using namespace std;

//todo
void get_URL( const string& host, const string& path )
{
  cerr << "Function called: get_URL(" << host << ", " << path << ")\n";
  // cerr << "Warning: get_URL() has not been implemented yet.\n";
  Address addr(host,"http");
  TCPSocket socket;
  socket.connect(addr);

  std::string request="GET "+path+" HTTP/1.1\r\n"+
  "Host: "+host+"\r\n"+
  "Connection: close\r\n"+
  "\r\n";

  socket.write(request);
  //注意：声明了buffers后要分配缓区不然无法读取数据
 vector<string> buffers;

  while(!socket.eof())
  {
    sleep(1);
    buffers.clear();
    buffers.emplace_back();                 // 至少一个缓冲区
    buffers.back().resize(4096);            // 指定读取大小
 

    socket.read(buffers);

    // 如果本次读取为空，继续检查是否到 EOF
    if (buffers[0].empty()) {
        break;
    }

    for (const auto &buffer : buffers) {
        cout << buffer;
      
       
    }

   
  }

 
  socket.close();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
