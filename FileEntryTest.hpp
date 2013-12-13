#include "DetailBFS.hpp"
#include "DetailFileBlock.hpp"
#include "FileEntry.hpp"
#include "MakeBFS.hpp"
#include "TestHelpers.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>

#include <cassert>
#include <sstream>
#include <fstream>

class FileEntryTest
{
  public:
    FileEntryTest() : m_uniquePath(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path())
    {
        boost::filesystem::create_directories(m_uniquePath);
        testFileSizeReportedCorrectly();
        testBlocksAllocated();
        testFileUnlink();
        testBigWriteFollowedByRead();
        testBigWriteFollowedBySmallAppend();
        testSmallWriteFollowedByBigAppend();
        testSeekAndReadSmallFile();
        testWriteBigDataAppendSmallStringSeekToAndReadAppendedString();
    }

    ~FileEntryTest()
    {
        boost::filesystem::remove_all(m_uniquePath);
    }

  private:

    boost::filesystem::path m_uniquePath;

    void testFileSizeReportedCorrectly()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // test write get file size from same entry
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();
            ASSERT_EQUAL(BIG_SIZE, entry.fileSize(), "testFileSizeReportedCorrectly A");
        }

        // test get file size different entry but same data
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, uint64_t(1));
            ASSERT_EQUAL(BIG_SIZE, entry.fileSize(), "testFileSizeReportedCorrectly B");
        }
    }

    void testBlocksAllocated()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // test write get file size from same entry
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();

            uint64_t startBlock = entry.getStartBlockIndex();
            std::fstream in(testPath.c_str(), std::ios::in | std::ios::out | std::ios::binary);
            ASSERT_EQUAL(true, bfs::detail::isBlockInUse(startBlock, blocks, in), "testBlocksAllocated: blockAllocated");
            bfs::FileBlock block(testPath.c_str(), blocks, startBlock);
            uint64_t nextBlock = block.getNextIndex();
            while(nextBlock != startBlock) {
                startBlock = nextBlock;
                ASSERT_EQUAL(true, bfs::detail::isBlockInUse(startBlock, blocks, in), "testBlocksAllocated: blockAllocated");
                bfs::FileBlock block(testPath.c_str(), blocks, startBlock);
                nextBlock = block.getNextIndex();
            }
        }
    }

    void testFileUnlink()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // for storing block indices to make sure they've been deallocated after unlink
        std::vector<uint64_t> blockIndices;

        // test write followed by unlink
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();

            // get allocated block indices
            uint64_t startBlock = entry.getStartBlockIndex();
            std::fstream in(testPath.c_str(), std::ios::in | std::ios::out | std::ios::binary);
            blockIndices.push_back(startBlock);
            bfs::FileBlock block(testPath.c_str(), blocks, startBlock);
            uint64_t nextBlock = block.getNextIndex();
            while(nextBlock != startBlock) {
                startBlock = nextBlock;
                bfs::FileBlock block(testPath.c_str(), blocks, startBlock);
                blockIndices.push_back(startBlock);
                nextBlock = block.getNextIndex();
            }
            in.close();

            // no unlink and assert that file size is 0
            entry.unlink();
            ASSERT_EQUAL(0, entry.fileSize(), "testFileUnlink A");
        }

        // test that filesize is 0 when read back in
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            ASSERT_EQUAL(0, entry.fileSize(), "testFileUnlink B");

            // test that blocks deallocated after unlink
            std::vector<uint64_t>::iterator it = blockIndices.begin();
            std::fstream in(testPath.c_str(), std::ios::in | std::ios::out | std::ios::binary);
            for(; it != blockIndices.end(); ++it) {
                ASSERT_EQUAL(false, bfs::detail::isBlockInUse(*it, blocks, in), "testFileUnlink: blockDeallocatedTest");
            }
            in.close();
        }


    }

    void testBigWriteFollowedByRead()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // test write
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), testData.length());
            entry.flush();
        }

        // test read
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, uint64_t(1));
            std::string expected(createLargeStringToWrite());
            std::vector<char> vec;
            vec.resize(entry.fileSize());
            entry.read(&vec.front(), entry.fileSize());
            std::string recovered(vec.begin(), vec.begin() + entry.fileSize());
            assert(recovered == expected);
            ASSERT_EQUAL(recovered, expected, "testWriteFollowedByRead");
        }
    }

    void testBigWriteFollowedBySmallAppend()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // initial big write
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();
        }

        // small append
        std::string appendString("appended!");
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt", 1);
            entry.write(appendString.c_str(), appendString.length());
            entry.flush();
        }

        // test read
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, 1);
            std::string expected(createLargeStringToWrite());
            expected.append(appendString);
            std::vector<char> vec;
            vec.resize(BIG_SIZE + appendString.length());
            entry.read(&vec.front(), BIG_SIZE + appendString.length());
            std::string recovered(vec.begin(), vec.begin() + BIG_SIZE + appendString.length());
            ASSERT_EQUAL(recovered, expected, "testBigWriteFollowedBySmallAppend");
        }
    }

    void testSmallWriteFollowedByBigAppend()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // initial small write
        std::string testData("small string");
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), testData.length());
            entry.flush();
        }

        // big append
        std::string appendString(createLargeStringToWrite());
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt", 1);
            entry.write(appendString.c_str(), appendString.length());
            entry.flush();
        }

        // test read
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, 1);
            std::string expected(testData.append(appendString));
            std::vector<char> vec;
            vec.resize(entry.fileSize());
            entry.read(&vec.front(), entry.fileSize());
            std::string recovered(vec.begin(), vec.begin() + entry.fileSize());
            ASSERT_EQUAL(recovered, expected, "testSmallWriteFollowedByBigAppend");
        }
    }

    void testSeekAndReadSmallFile()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // test write
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string const testString("Hello and goodbye!");
            std::string testData(testString);
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();
        }

        // test seek and read
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, 1);
            std::string expected("goodbye!");
            std::vector<char> vec;
            vec.resize(expected.size());
            entry.seek(10);
            entry.read(&vec.front(), expected.size());
            std::string recovered(vec.begin(), vec.end());
            ASSERT_EQUAL(recovered, expected, "testSeekAndReadSmallFile");
        }
    }

    void testWriteBigDataAppendSmallStringSeekToAndReadAppendedString()
    {
        long const blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        // test write
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt");
            std::string testData(createLargeStringToWrite());
            std::vector<uint8_t> vec(testData.begin(), testData.end());
            entry.write((char*)&vec.front(), BIG_SIZE);
            entry.flush();
        }

        // test seek of big file
        std::string appendString("appended!");
        {
            bfs::FileEntry entry(testPath.c_str(), blocks, "test.txt", 1);
            entry.write(appendString.c_str(), appendString.length());
            entry.flush();
        }

        {
            bfs::FileEntry entry(testPath.c_str(), blocks, 1);
            std::vector<char> vec;
            vec.resize(appendString.length());
            entry.seek(BIG_SIZE);
            entry.read(&vec.front(), appendString.length());
            std::string recovered(vec.begin(), vec.end());
            ASSERT_EQUAL(recovered, appendString, "testWriteBigDataAppendSmallStringSeekToAndReadAppendedString");
        }

    }

};