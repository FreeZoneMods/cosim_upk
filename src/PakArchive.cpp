#include "PakArchive.h"

#include "FileReader.h"
#include "Utils.h"

#define DEF_WBITS -15
#include "zlib\zlib.h"
#include <memory.h>
#include <cassert>

CPakArchive::CPakArchive(void):        
        m_loaded( false ),
        m_filesCount( 0 ),
        m_chunksCount( 0 ),
        m_nameRegistryTable( nullptr ),
        m_filesTable( nullptr ),
        m_chunksTable( nullptr ),
        m_chunksBlobSize( 0 ),
        m_chunksBlob( nullptr ) {
}

CPakArchive::~CPakArchive(void) {
    Unload();
}

void CPakArchive::Unload() {    
    delete[] m_nameRegistryTable;
    delete[] m_filesTable;
    delete[] m_chunksTable;
    delete[] m_chunksBlob;
    m_nameRegistryTable = nullptr;
    m_filesTable = nullptr;
    m_filesTable = nullptr;
    m_chunksBlob = nullptr;
    m_chunksTable = 0;
    m_chunksCount = 0;
    m_chunksBlobSize = 0;
    m_loaded = false;
}

bool CPakArchive::Load( const std::string& fileName ) {
    Unload();

    // Read header
    CFileReader reader( fileName );
    if ( !reader.ReadAndCheck( HEADER_MAGIC ) || !reader.ReadAndCheck( HEADER_VERSION ) || !reader.ReadAndCheck( HEADER_PLATFORM ) || !reader.Read( m_filesCount ) || ( m_filesCount== 0 ) || !reader.ReadAndCheck( HEADER_VERSION2 ) ) {
        return false;
    }

    // Read name registry
    uint32 sizeofNameRegistryPacked, sizeofNameRegistryUnpacked;
    if ( !reader.Read( sizeofNameRegistryUnpacked ) || !reader.Read( sizeofNameRegistryPacked ) ) {
        return false;
    }

    size_t alignedNameRegistrySize = ALIGN_UP( sizeofNameRegistryPacked, PAK_ALIGNMENT );
    char* packedNameRegistry = new char [alignedNameRegistrySize ];
    if ( !reader.Read( packedNameRegistry, alignedNameRegistrySize ) ) {
        delete[] packedNameRegistry;
        return false;
    }

    m_nameRegistryTable = UnpackNameRegistry( packedNameRegistry, sizeofNameRegistryPacked, sizeofNameRegistryUnpacked, m_filesCount );
    delete[] packedNameRegistry;

    if ( m_nameRegistryTable == nullptr ) {
        return false;
    }

    uint32* unknownIndexTable = new uint32[m_filesCount];
    reader.Read( unknownIndexTable, sizeof(uint32) * m_filesCount );
    delete[] unknownIndexTable;

    // Read files table
    uint32 packedFileTableSize;
    if ( !reader.Read( packedFileTableSize ) ) {
        return false;
    }    
    char* packedFileTable = new char [ALIGN_UP( packedFileTableSize, PAK_ALIGNMENT )];
    if ( !reader.Read( packedFileTable, ALIGN_UP( packedFileTableSize, PAK_ALIGNMENT ) ) ) {
        delete[] packedFileTable;
        return false;
    }
    uint32 gamma_for_filetable = m_filesCount + packedFileTableSize;
    *( (uint32*) &packedFileTable[0] ) ^= gamma_for_filetable; //decrypt zlib compress mode
    m_filesTable = new SFileTableEntry[m_filesCount];
    size_t unpackedFileTableSize = sizeof(SFileTableEntry) * m_filesCount;
    if ( !UnpackBuffer( packedFileTable, packedFileTableSize, m_filesTable, unpackedFileTableSize ) ) {
        delete[] packedFileTable;
        return false;
    }
    delete[] packedFileTable;

    // Read chunks table
    uint32 packedChunksTableSize;
    if ( !reader.Read( packedChunksTableSize ) || !reader.Read( m_chunksCount ) ) {
        return false;
    }
    char* packedChunksTable = new char [ALIGN_UP( packedChunksTableSize, PAK_ALIGNMENT )];
    if ( !reader.Read( packedChunksTable, ALIGN_UP( packedChunksTableSize, PAK_ALIGNMENT ) ) ) {
        delete[] packedChunksTable;
        return false;
    }
    uint32 gamma_for_chunks = m_chunksCount + m_filesCount + packedChunksTableSize;
    *( (uint32*) &packedChunksTable[0] ) ^= gamma_for_chunks; //decrypt zlib compress mode
    m_chunksTable = new SChunkTableEntry[m_chunksCount];
    size_t unpackedChunksTableSize = sizeof(SChunkTableEntry) * m_chunksCount;
    if ( !UnpackBuffer( packedChunksTable, packedChunksTableSize, m_chunksTable, unpackedChunksTableSize ) ) {
        delete[] packedChunksTable;
        return false;
    }
    delete[] packedChunksTable;

    // Read chunks' data blob
    m_chunksBlobSize = reader.BytesLeftCount();
    m_chunksBlob = new char[m_chunksBlobSize];
    if ( !reader.Read( m_chunksBlob, m_chunksBlobSize) ) {
        delete[] m_chunksBlob;
        return false;
    } 

    m_loaded = true;
    return true;
}

bool CPakArchive::UnpackBuffer( const void* packedBuffer, size_t packedSize, void* unpackedBuffer, size_t& unpackedSize ) {
    bool result = false;
    size_t realUnpackedSize = unpackedSize;
    result = ( uncompress( (Bytef*) unpackedBuffer, (uLongf*) &realUnpackedSize, (Bytef*) packedBuffer, packedSize ) == Z_OK );
    if ( result ) {
        unpackedSize = realUnpackedSize;
    }
    return result;
}

char* CPakArchive::UnpackNameRegistry( const void* packedBuffer, size_t packedSize, size_t unpackedSize, uint32 entriesCount ) {
    if ( ( packedBuffer == nullptr ) || ( packedSize < sizeof( entriesCount ) || ( entriesCount == 0 ) ) ) {
        return nullptr;
    }
    
    bool result = false;
    char* packedBytes = new char[packedSize];
    memcpy( packedBytes, packedBuffer, packedSize );
    
    // 1. zlib settings bytes XORed with entries count
    *( (uint32*) packedBytes ) ^= entriesCount;
    
    // 2. Use zlib to uncompress info
    char* unpackedBytes = new char[unpackedSize];
    size_t reallyUnpacked = unpackedSize;
    if ( !UnpackBuffer( packedBytes, packedSize, unpackedBytes, reallyUnpacked ) && ( reallyUnpacked == unpackedSize ) ) {
        delete[] packedBytes;
        delete[] unpackedBytes;
        return nullptr;
    }
    delete[] packedBytes;

    // 3. "Decrypt" names (yes, they use compression after crypting data in packing process...)
    // and copy strings with pathes    
    char* curPtr = unpackedBytes;
    for ( uint32 i = 0; i < entriesCount; ++i ) {
        uint32 entryLength = *( (uint32*) curPtr );
        curPtr += sizeof( entryLength );
        uint32 gamma = i + entryLength % 5;
        for ( uint32 j = 0; j < entryLength; ++j ) {
            *( curPtr + j )  ^= gamma + 2 * ( j + entryLength );
        }        
        curPtr += entryLength + 1; //Strings are null-terminated
    }           
    return unpackedBytes;
}

const char* CPakArchive::GetFileName( uint32 index ) const {
    assert( m_loaded && index < m_filesCount );
    SFileTableEntry* curEntry = & ( m_filesTable[index] );
    return &( m_nameRegistryTable[curEntry->fileNameOffset] );
}

size_t CPakArchive::GetFileSize( uint32 index ) const {
    assert( m_loaded && index < m_filesCount );
    SFileTableEntry* curEntry = & ( m_filesTable[index] );
    return curEntry->fileSize;
}

bool CPakArchive::GetFile( uint32 index, void* buffer, size_t bufferSize ) const {
    assert( m_loaded && index < m_filesCount );
    SFileTableEntry* curEntry = & ( m_filesTable[index] );

    if ( curEntry->fileSize < bufferSize ) {
        return false;
    }
    
    bool result = true;
    for ( uint32 processedChunks = 0, curChunkIdx = curEntry->startChunkIndex; 
                result && processedChunks < curEntry->chunksCount; 
                ++processedChunks, ++curChunkIdx ) {
        SChunkTableEntry* curChunk = &(m_chunksTable[curChunkIdx]);
        char* outPosPtr = &( (char*) buffer )[curChunk->offsetFromFileStart];
        if ( curChunk->packedSize == curChunk->unpackedSize ) {
            memcpy( outPosPtr, &m_chunksBlob[curChunk->startOffset], curChunk->unpackedSize ) ;
        } else {
            size_t unpackedSz = curChunk->unpackedSize;
            result = UnpackBuffer( &m_chunksBlob[curChunk->startOffset], curChunk->packedSize, outPosPtr, unpackedSz );
        }
    }
    return result;
}