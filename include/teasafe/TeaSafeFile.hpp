/*
  Copyright (c) <2013-2014>, <BenHJ>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef TeaSafe_TEASAFE_FILE_HPP__
#define TeaSafe_TEASAFE_FILE_HPP__

#include "teasafe/CoreTeaSafeIO.hpp"
#include "teasafe/FileBlock.hpp"
#include "teasafe/OpenDisposition.hpp"

#include <boost/function.hpp>
#include <boost/optional.hpp>

#include <vector>

#include <iosfwd>                           // streamsize, seekdir

#include <string>

#include <deque>

namespace teasafe
{

    class TeaSafeFile
    {

        typedef boost::function<void(uint64_t)> SetEntryInfoSizeCallback;
        typedef boost::optional<SetEntryInfoSizeCallback> OptionalSizeCallback;

      public:
        /**
         * @brief when creating a new file this constructor should be used
         * @param io the core teasafe io (path, blocks, password)
         * @param name the name of the file entry
         * @param enforceStartBlock true if start block should be set
         */
        TeaSafeFile(SharedCoreIO const &io, std::string const &name, bool const enforceStartBlock = false);

        /**
         * @brief when reading or appending or overwriting to the end of a file
         * this constructor should be used
         * @param io the core teasafe io (path, blocks, password)
         * @param name the name of the file entry
         * @param block the starting block of the file entry
         * @param openDisposition open mode
         */
        TeaSafeFile(SharedCoreIO const &io,
                    std::string const &name,
                    uint64_t const startBlock,
                    OpenDisposition const &openDisposition);

        typedef char                                   char_type;
        typedef boost::iostreams::seekable_device_tag  category;

        /**
         * @brief  simply acceses the name of the teasafe file
         * @return the teasafe file's name
         */
        std::string filename() const;

        /**
         * @brief  accesses file size
         * @return file size in bytes
         */
        uint64_t fileSize() const;

        /**
         * @brief  retrieves the first file block making up this teasafe file
         * @return the start block index of this file
         */
        uint64_t getStartVolumeBlockIndex() const;

        /**
         * @brief truncates a file to new size
         * @param newSize the new fileSize
         */
        void truncate(std::ios_base::streamoff newSize);

        /**
         * @brief  for reading bytes from the teasafe file
         * @param  s buffer to store the read bytes
         * @param  n number of bytes to read
         * @return number of bytes read
         */
        std::streamsize read(char* s, std::streamsize n);

        /**
         * @brief  for writing bytes to the teasafe file
         * @param  s buffer that stores the bytes to write
         * @param  n the number of bytes to write
         * @return the number of bytes written
         */
        std::streamsize write(const char* s, std::streamsize n);

        /**
         * @brief  allows seeking to a given position in the teasafe file
         * @param  off the offset to seek to
         * @param  way the position of where to offset from (begin, current, or end)
         * @return returns the offset (NOTE: should this be returning the actual
         * 'tell' position instead?
         */
        boost::iostreams::stream_offset seek(boost::iostreams::stream_offset off,
                                             std::ios_base::seekdir way = std::ios_base::beg);

        /**
         * @brief indicates the stream position
         * @return the current stream position
         */
        boost::iostreams::stream_offset tell() const;

        /**
         * @brief flushes any remaining data
         */
        void flush();

        /**
         * @brief deallocates blocks associated with this file entry; used
         * in conjunction with deleting the file
         */
        void unlink();

        /**
         * @brief sets the callback that will be used to updated the reported
         * file size as stored in the entry info metadata of the parent
         * @param callback the setSize function (probably) of EntryInfo
         */
        void setOptionalSizeUpdateCallback(SetEntryInfoSizeCallback callback);

      private:

        // the core teasafe io (path, blocks, password)
        SharedCoreIO m_io;

        // the name of the file entry
        std::string m_name;

        // whether to enforce the starting file block (potentially dangerous!)
        mutable bool m_enforceStartBlock;

        // the size of the file
        uint64_t m_fileSize;

        // the file blocks making up the file
        mutable std::vector<FileBlock> m_fileBlocks;

        // a buffer used for storing chunks of data
        std::vector<uint8_t> m_buffer;

        // the index of the current file block being read from or written to
        // note, this is the position of the block in the teasafe
        mutable uint64_t m_currentVolumeBlock;

        // the start file block index
        mutable uint64_t m_startVolumeBlock;

        // the index of the block in the actual blocks container;
        // in comparison to m_currentBlock, this is where the block
        // exists in m_fileBlocks
        mutable int64_t m_blockIndex;

        // open mode
        OpenDisposition m_openDisposition;

        // the current 'stream position' of file entry
        std::streamoff m_pos;

        // an optional size update callback to be used in setting the reported
        // size in the entry info held in the parent folder entry info cache
        OptionalSizeCallback m_optionalSizeCallback;

        /**
         * @brief  for keeping track of what the current file block is when
         *         reading or writing, it is stored in m_currentVolumeBlock
         * @return the current volume block
         */
        uint64_t getCurrentVolumeBlockIndex();

        /**
         * @brief buffers a byte for writing
         * @param byte the byte to buffer for writing
         */
        void bufferByteForWriting(char const byte);

        /**
         * @brief creates and pushes back a new file block for writing
         */
        void newWritableFileBlock() const;

        /**
         * @brief when appending, set all blocks in the block list
         * @note also updates file size as it seeks to end block
         * @param stream the teasafe image stream
         */
        void setBlocks();

        /**
         * @brief writes data to file block
         * @param bytes the number of bytes to write
         */
        void writeBufferedDataToBlock(uint32_t const bytes);

        /**
         * @brief reads bytes from the block in to buffer
         * @return the number of bytes read
         */
        std::streamsize readCurrentBlockBytes();


        /**
         * @brief will build a new file block for writing to if there are
         * no file blocks or if there are file blocks and it is determined
         * that we're not in append mode
         */
        void checkAndCreateWritableFileBlock() const;

        /**
         * @brief used in the context of discovering if currently set block
         * has enough space to write more data to
         * @return true if space availble, false otherwise
         */
        bool currentBlockHasAvailableSpace() const;

        /**
         * @brief sets next block index for each file block
         */
        void setBlockNextIndices();
    };

}

#endif // TeaSafe_TEASAFE_FILE_HPP__