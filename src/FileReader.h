#pragma once

#include <string>
#include <fstream>

class CFileReader
{
public:
    CFileReader( const std::string& fileName ) {
        m_stream.open( fileName, std::ios::binary );
    }
    ~CFileReader(void) {};

template <class T>
bool ReadAndCheck( const T& expected_value ) {
    T val;
    m_stream.read( (char*) &val, sizeof( T ) );
    return !( m_stream.fail() || m_stream.eof() || ( expected_value != val ) );  
}

template <class T>
bool Read( T& buf ) {    
    m_stream.read( (char*) &buf, sizeof( T ) );
    return !( m_stream.fail() || m_stream.eof() );  
}

bool Read( void* ptr, size_t size ) {    
    m_stream.read( (char*) ptr, size );
    return !( m_stream.fail() || m_stream.eof() );  
}

size_t BytesLeftCount() {
    std::streampos cur = m_stream.rdbuf()->pubseekoff( 0, m_stream.cur );
    std::streampos end = m_stream.rdbuf()->pubseekoff( 0, m_stream.end ); 
    m_stream.rdbuf()->pubseekoff( cur, m_stream.beg ); 
    return size_t(end)-size_t(cur);    
}

protected:
    std::ifstream m_stream;
};

