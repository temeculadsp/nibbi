#pragma once
#include "Platform.h"
#include <cstring>

// Read-only FATFS services for the unmodified streaming/request model. Files
// are decoded before publication. Each voice owns a cursor into immutable PCM;
// no operating-system file calls, locks, allocations or global file registry.
namespace daisy {
struct WAV_FormatTypeDef {
    uint32_t ChunkId, FileSize, FileFormat, SubChunk1ID, SubChunk1Size;
    uint16_t AudioFormat, NbrChannels;
    uint32_t SampleRate, ByteRate;
    uint16_t BlockAlign, BitPerSample;
    uint32_t SubChunk2ID, SubCHunk2Size;
};
static_assert(sizeof(WAV_FormatTypeDef) == 44);
constexpr uint32_t kWavFileChunkId=0x46464952, kWavFileWaveId=0x45564157,
    kWavFileSubChunk1Id=0x20746d66, kWavFileSubChunk2Id=0x61746164;
constexpr uint16_t WAVE_FORMAT_PCM=1;
using UINT = unsigned int;
enum FRESULT { FR_OK, FR_DISK_ERR };
constexpr int FA_OPEN_ALWAYS=1, FA_WRITE=2, FA_READ=4, FA_CREATE_ALWAYS=8;
struct FIL {
    struct { size_t objsize=0; } obj;
    const int16_t* pcm=nullptr;
    const int16_t* doublePcm=nullptr;
    size_t doubleCount=0;
    size_t count=0, cursor=0;
    bool doubled=false;
};
inline size_t f_size(const FIL* file) { return file->obj.objsize; }
inline size_t f_tell(const FIL* file) { return file->cursor; }
inline FRESULT f_open(FIL* file, const char* name, int) {
    if (!file->pcm || file->count < 4) return FR_DISK_ERR;
    file->doubled=std::strcmp(name,"double")==0;
    // Use the supplied factory double-speed file when present. For imports,
    // expose FileCopier's every-other-stereo-frame conversion virtually.
    const auto frames=file->count/2;
    file->obj.objsize=44+(file->doubled && file->doublePcm ? file->doubleCount*2 : (file->doubled ? frames/2 : frames)*4);
    file->cursor=0;
    return FR_OK;
}
inline FRESULT f_close(FIL* file) { file->obj.objsize=0; file->cursor=0; return FR_OK; }
inline FRESULT f_lseek(FIL* file, size_t offset) {
    if (offset>file->obj.objsize) return FR_DISK_ERR;
    file->cursor=offset;
    return FR_OK;
}
inline FRESULT f_read(FIL* file, void* dest, size_t bytes, UINT* read) {
    *read=0;
    if (file->cursor<44 || file->cursor>f_size(file) || (file->cursor-44)%4 || bytes%4)
        return FR_DISK_ERR;
    bytes=std::min(bytes,f_size(file)-file->cursor);
    auto* out=static_cast<int16_t*>(dest);
    const size_t first=(file->cursor-44)/2;
    if (file->doubled && file->doublePcm) std::memcpy(dest,file->doublePcm+first,bytes);
    else if (!file->doubled) std::memcpy(dest,file->pcm+first,bytes);
    else for(size_t i=0;i<bytes/2;i+=2) {
        out[i]=file->pcm[(first+i)*2]; out[i+1]=file->pcm[(first+i)*2+1];
    }
    file->cursor+=bytes;
    *read=static_cast<UINT>(bytes);
    return FR_OK;
}
// Writes belong to the desktop capture/catalog layer, never this read service.
inline FRESULT f_write(FIL*,const void*,size_t,UINT*) { return FR_DISK_ERR; }
inline FRESULT f_sync(FIL*) { return FR_DISK_ERR; }
inline FRESULT f_unlink(const char*) { return FR_DISK_ERR; }
inline FRESULT f_truncate(FIL*) { return FR_DISK_ERR; }
}
