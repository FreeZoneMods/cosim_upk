#pragma once

#include "PlatformSpecific.h"
#include <vector>

class CPakArchive {
public:
    struct SFileTableEntry {
        uint32 fileSize;
        uint32 fileNameOffset;
        uint32 chunksCount;
        uint32 startChunkIndex;
    };
    
    struct SChunkTableEntry {
        uint32 offsetFromFileStart;
        uint32 unpackedSize;
        uint32 startOffset;
        uint32 packedSize;
    };

    CPakArchive(void);
    ~CPakArchive(void);

    bool CPakArchive::Load( const std::string& fileName );

    uint32 GetFilesCount() const { 
        return m_filesCount;
    }

    const char* GetFileName( uint32 index ) const;

    size_t GetFileSize( uint32 index ) const;

    bool GetFile( uint32 index, void* buffer, size_t bufferSize ) const;
  
    static bool UnpackBuffer( const void* packedBuffer, size_t packedSize, void* unpackedBuffer, size_t& unpackedSize );
    
private:
    void Unload();

    // Unpacks and decrypts name registry
    static char* UnpackNameRegistry( const void* packedBuffer, size_t packedSize, size_t unpackedSize, uint32 entriesCount );

    bool m_loaded;
    uint32 m_filesCount;
    uint32 m_chunksCount;  
    char* m_nameRegistryTable;
    SFileTableEntry* m_filesTable;
    SChunkTableEntry* m_chunksTable;

    uint32 m_chunksBlobSize;
    char* m_chunksBlob;

    static const uint32 HEADER_MAGIC = 0x4B415054;
    static const uint32 HEADER_VERSION = 0x7;
    static const uint32 HEADER_VERSION2 = 0xFFFFFFE3;
    static const uint32 HEADER_PLATFORM = 0x0;

    static const uint32 PAK_ALIGNMENT = 4;
};

