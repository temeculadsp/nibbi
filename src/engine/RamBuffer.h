#pragma once
#include <atomic>
#include <vector>
#include "../dsp/Platform.h"


namespace daisy
{

    static const size_t kMaxRamBuffSize = 31694848 / 2;
    // total size = 31694848, which goes evenly into 8K, which means SDRAM page alignment

    // Desktop adaptation: lock-free sample access lets the host save a loop
    // while playback continues, without a data race or an audio-thread lock.
    struct AtomicSample {
        std::atomic<int16_t> value{0};
        operator int16_t() const { return value.load(std::memory_order_relaxed); }
        AtomicSample& operator=(int16_t x) { value.store(x, std::memory_order_relaxed); return *this; }
    };
    static_assert(std::atomic<int16_t>::is_always_lock_free, "Tape samples must be lock-free");
    // just a buffer with its length
    struct RamBufferMemory
    {
        // pointer to array of size kMaxRamBuffSize
        void Init(AtomicSample* m)
        {
            mem = m;
        }

        AtomicSample* mem;
        const int16_t* read_only = nullptr;
        int16_t* write_view = nullptr;
        size_t capacity = kMaxRamBuffSize;
        void Write(size_t i,int16_t value) { if(write_view) write_view[i]=value; else mem[i]=value; }
        int16_t Read(size_t i) const { return read_only ? read_only[i] : write_view ? write_view[i] : int16_t(mem[i]); }
        std::atomic<size_t> length{0};

        void Clear()
        {
            std::fill(&mem[0], &mem[kMaxRamBuffSize], 0);
        }
    };

    // keeps track of its own read and write heads
    class RamBuffer
    {
    public:
        void Init(RamBufferMemory* b)
        {
            buff = b;
            buff->Clear();
        }

        void Attach(RamBufferMemory* b) { buff = b; read_head = write_head = 0; }

        void Reset()
        {
            buff->length = read_head = write_head = 0;
        }

        void BlockRead(int16_t* copy_buff, size_t size)
        {
            if(read_head + size > buff->capacity)
                size = buff->capacity - read_head;
            if(read_head + size > buff->length)
                size = buff->length - read_head;

            for(size_t i=0;i<size;++i) copy_buff[i]=buff->Read(read_head+i);
            read_head += size;
        }

        void StereoRead(int16_t* l, int16_t* r, bool rev)
        {
            const size_t new_read_head = read_head + 2;
            if(!rev && new_read_head <= buff->length && new_read_head <= buff->capacity)
            {
                *l = buff->Read(read_head);
                *r = buff->Read(read_head + 1);
                read_head = new_read_head;
            }
            else if(rev && read_head >= 2)
            {
                *l = buff->Read(read_head - 2);
                *r = buff->Read(read_head - 1);
                read_head -= 2;
            }
            else
            {
                *l = *r = 0;
            }
        }

        void BlockWrite(int16_t* copy_buff, size_t size)
        {
            if(write_head + size > buff->capacity)
                size = buff->capacity - write_head;

            for(size_t i=0;i<size;++i) buff->Write(write_head+i,copy_buff[i]);
            write_head += size;
            buff->length = write_head;
            buff->length -= buff->length % 2;
        }

        void StereoWrite(int16_t l, int16_t r, bool rev, bool set_len)
        {
            if(!rev)
            {
                const size_t new_write_head = write_head + 2;
                if(new_write_head <= buff->capacity)
                {
                    buff->Write(write_head,l);
                    buff->Write(write_head + 1,r);
                    write_head = new_write_head;
                    if(set_len)
                        buff->length = write_head;
                }
            }
            else if(write_head >= 2)
            {
                buff->Write(write_head - 2,l);
                buff->Write(write_head - 1,r);
                write_head -= 2;
            }
        }

        inline size_t GetSize() { return buff->length; }

        inline size_t GetReadHead() { return read_head; }
        inline bool ReadEOF() { return read_head >= buff->length || read_head >= buff->capacity; }
        inline bool ReadLoop(bool rev) { return (rev && read_head == 0) || (!rev && ReadEOF()); }

        void SetReadHead(size_t pos) 
        { 
            if(pos <= buff->capacity && pos <= buff->length)
                read_head = pos;
        }
        
        size_t GetRemainingRead(bool rev)
        {
            if(!rev && !ReadEOF())
                return buff->length - read_head;
            else if(rev)
                return read_head;

            return 0;
        }

        inline size_t GetWriteHead() { return write_head; }
        inline bool WriteEOF() { return write_head >= buff->length || write_head >= buff->capacity; }
        inline bool WriteFullLength() { return write_head >= buff->capacity; }
        inline bool WriteLoop(bool rev) { return (rev && write_head == 0) || (!rev && WriteEOF()); }

        void SetWriteHead(size_t pos) 
        {
            if(pos <= buff->capacity && pos <= buff->length)
                write_head = pos;
        }

        size_t GetRemainingWrite(bool rev)
        {
            if(!rev && !WriteEOF())
                return buff->length - write_head;
            else if(rev)
                return write_head;

            return 0;            
        }

        void AdvanceWrite(bool rev)
        {
            if(rev && write_head >= 2)
                write_head -= 2;
            else if(!rev && write_head + 2 <= buff->length && write_head + 2 < buff->capacity)
                write_head += 2;
        }

    private:
        RamBufferMemory* buff;
        size_t read_head, write_head;
    };
} // namespace daisy