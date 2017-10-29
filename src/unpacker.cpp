#include "PakArchive.h"
#include <iostream>
#include <string>

#include "windows.h"
#include <fstream>

bool UnpackGamePakArchive( const char* fileName )
{
    CPakArchive pak;
    bool res = pak.Load( fileName );
    for( uint32 idx = 0; res && idx < pak.GetFilesCount(); ++idx )
    {
        std::string fileName = pak.GetFileName( idx );
        size_t sz = pak.GetFileSize(idx);
        std::cout << "Filename " << fileName.c_str() << ", Size " << sz << std::endl;  
        
        for ( size_t i = 0; i < fileName.length(); ++i ) {
            if ( fileName[i] == '\\' ) {
                std::string subpath = fileName.substr( 0, i );
                int res = CreateDirectoryA( subpath.c_str(), nullptr );                
            }
        }
        
        char* fl = new char[sz];
        res &= pak.GetFile( idx, fl, sz );
        if ( res ) {
            std::ofstream out( fileName, std::ios::binary );
            out.write( fl, sz );
            if ( out.fail() ) {
                std::cout << "ERROR: can't write unpacked file" << std::endl;
            }
        }
        else {
            std::cout << "ERROR: unpacking failed" << std::endl;
        }

        delete fl;
    }
    return res;
}


int main(int argc, char* argv[])
{
    std::ifstream fileList( "datasources.txt" );
    if ( fileList.fail() ) {
        std::cout << "Can't open file 'datasources.txt'" << std::endl;
        return -1;
    }

    do {
        std::string fileName;
        std::getline( fileList, fileName ); 
        std::cout << "------Unpacking '" << fileName.c_str() << "'------" << std::endl;
        bool res = UnpackGamePakArchive( fileName.c_str() );
        if ( res ) {
            std::cout << "------Unpacked '" << fileName.c_str() << "'------" << std::endl;
        } else {
            std::cout << "------Unpacking failed for '" << fileName.c_str() << "'------" << std::endl;
        }
    } while (!fileList.eof());

    std::cout << "Done!" << std::endl;
    std::cin.get();
    return 0;    
}

