#include "uopfile.h"

#include <cstring> // for memset
#include <sstream>
#include "zlib.h"

#include "uophash.h"

#define ADDERROR(str) UOPError::append((str), errorQueue)

enum class ZLibQuality
{
    None		= 0,
    Speed		= 1,
    Medium      = 5,
    Best		= 9//,
    //Default	= -1,
};


namespace uopp
{


UOPFile::UOPFile(UOPBlock *parent, unsigned int index) :
    m_parent(parent), m_index(index),
    m_dataBlockAddress(0), m_dataBlockLength(0), m_compressedSize(0), m_decompressedSize(0),
    m_fileHash(0), m_dataBlockHash(0), m_compression(CompressionFlag::Uninitialized),
    m_added(false)
{
}

//UOPFile::~UOPFile()
//{
//}


//--

bool UOPFile::read(std::ifstream& fin, UOPError *errorQueue)
{
    //m_Parent = parent;

    // Read file's header
    fin.read(reinterpret_cast<char*>(&m_dataBlockAddress), 8);
    fin.read(reinterpret_cast<char*>(&m_dataBlockLength), 4);
    fin.read(reinterpret_cast<char*>(&m_compressedSize), 4);
    fin.read(reinterpret_cast<char*>(&m_decompressedSize), 4);
    fin.read(reinterpret_cast<char*>(&m_fileHash), 8);
    fin.read(reinterpret_cast<char*>(&m_dataBlockHash), 4);     // adler32 hash of the compressed data?

    short comprFlag = 0;
    fin.read(reinterpret_cast<char*>(&comprFlag), 2);

    switch ( comprFlag )
    {
        case 0x0: m_compression = CompressionFlag::None; break;
        case 0x1: m_compression = CompressionFlag::ZLib; break;
        default:
            ADDERROR("Unsupported compression type " + std::to_string(comprFlag));
            return false;
    }
    return true;
}

bool UOPFile::readPackedData(std::ifstream& fin, UOPError*)
{
    m_data.resize(m_compressedSize);
    memset(m_data.data(), 0, m_compressedSize);

    fin.seekg(std::streamoff(m_dataBlockAddress + m_dataBlockLength), std::ios_base::beg);
    fin.read(m_data.data(), m_compressedSize * sizeof(char));
    return !fin.bad();
}

void UOPFile::freePackedData()
{
    m_data.clear();
    m_data.shrink_to_fit();
}

bool UOPFile::unpack(std::vector<char>* decompressedData, UOPError *errorQueue)
{
    if (m_data.empty())
        return false;

    switch ( m_compression )
    {
        case CompressionFlag::ZLib:
        {
            decompressedData->resize(m_decompressedSize);
            uLongf destLength = m_decompressedSize;

            int z_result = ::uncompress(reinterpret_cast<Bytef*>(decompressedData->data()), &destLength,
                                        reinterpret_cast<const Bytef*>(m_data.data()), m_compressedSize );

            bool success = true;
            if (z_result != Z_OK)
            {
                ADDERROR(translateZlibError(z_result));
                if (destLength != uLongf(m_decompressedSize))
                    ADDERROR("ZLib: Different decompressed size!");
                //else
                    success = false;
            }
            //resultSize = (size_t)destLength;
            return success;
        }

        case CompressionFlag::None:
            *decompressedData = m_data;
            return true;

        default:
            ADDERROR("Invalid compression flag for UOPFile::unpack: " + std::to_string(short(m_compression)) + " (" + std::to_string(m_fileHash) + ")");
            return false;
    }
}


//--

bool UOPFile::compressAndReplaceData(const std::vector<char>* sourceDecompressed, CompressionFlag compression, bool addDataHash, UOPError* errorQueue)
{
    if (compression == CompressionFlag::Uninitialized)
    {
        ADDERROR("Invalid compression flag for UOPFile::compressAndReplaceData");
        return false;
    }

    m_compression = compression;
    m_decompressedSize = unsigned(sourceDecompressed->size());
    if (compression == CompressionFlag::None)
    {
        m_compressedSize = m_decompressedSize;
        m_data = *sourceDecompressed;
        m_dataBlockHash = addDataHash ? hashDataBlock(m_data.data(), m_data.size()) : 0;
        return true;
    }

    m_data.clear();
    m_data.resize(::compressBound( uLong(sourceDecompressed->size()) ));

    uLongf compressedSizeTemp = uLongf(m_data.size());
    int error = ::compress2(reinterpret_cast<Bytef*>(m_data.data()), &compressedSizeTemp,
                            reinterpret_cast<const Bytef*>(sourceDecompressed->data()), uLong(sourceDecompressed->size()),
                            int(ZLibQuality::Speed) );
    m_compressedSize = unsigned(compressedSizeTemp);

    if (error != Z_OK)
    {
        std::stringstream ssErr; ssErr << "ZLib compression error number " << error << ". Aborting.";
        ADDERROR(ssErr.str());
        m_fileHash = m_decompressedSize = m_compressedSize = m_dataBlockHash = 0;
        m_compression = CompressionFlag::Uninitialized;
        m_data.clear();
        m_data.shrink_to_fit();
        return false;
    }

    m_data.resize(m_compressedSize);
    m_dataBlockHash = addDataHash ? hashDataBlock(m_data.data(), m_data.size()) : 0;

    return true;
}

bool UOPFile::createFile(std::ifstream& fin, unsigned long long fileHash, CompressionFlag compression, bool addDataHash, UOPError *errorQueue)    // create file in memory
{
    std::stringstream ssHash; ssHash << std::hex << fileHash;
    std::string strHash("0x" + ssHash.str());
    if (fileHash == 0)
    {
        ADDERROR("Invalid fileHash for UOPFile::createFile (" + strHash + ")");
        return false;
    }
    if (compression == CompressionFlag::Uninitialized)
    {
        ADDERROR("Invalid compression flag for UOPFile::createFile: " + std::to_string(short(compression)) + " (" + strHash + ")");
        return false;
    }
    if (fin.bad())
    {
        ADDERROR("Bad filestream for UOPFile::createFile");
        return false;
    }
    m_added = true;

    //m_dataBlockAddress        // To be filled later, in UOPPackage::finalizeAndSave();
    //m_dataBlockLength         // To be filled later, in UOPPackage::finalizeAndSave();
    m_fileHash = fileHash;      // Hashed file name

    // Get the input file size
    std::streampos curPos = fin.tellg();
    fin.seekg(0, std::ios_base::end);
    std::streampos endPos = fin.tellg();
    fin.seekg(curPos, std::ios_base::beg);
    std::streamsize finSizeToRead = std::streamsize(endPos - curPos);

    // Write the raw file data in internal buffer
    std::vector<char> finData;
    finData.resize(size_t(finSizeToRead));  // don't use reserve, or ifstream.read won't work!
    //fin.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
    fin.read(finData.data(), finSizeToRead);

    return compressAndReplaceData(&finData, compression, addDataHash, errorQueue);
}

bool UOPFile::createFile(std::ifstream& fin, const std::string& packedFileName, CompressionFlag compression, bool addDataHash, UOPError *errorQueue)  // create file in memory
{
    if (packedFileName.empty())
    {
        ADDERROR("Invalid packedFileName for UOPFile::createFile (" + packedFileName + ")");
        return false;
    }
    if (compression == CompressionFlag::Uninitialized)
    {
        ADDERROR("Invalid compression flag for UOPFile::createFile: " + std::to_string(short(compression)) + " (" + packedFileName + ")");
        return false;
    }

    unsigned long long fileHash = hashFileName(packedFileName);
    return createFile(fin, fileHash, compression, addDataHash, errorQueue);
}


} // end of uopp namespace
