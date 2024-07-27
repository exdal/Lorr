#include "Compression.hh"

#include <lz4.h>
#include <zstd.h>

namespace lr {
CompressorLZ4::CompressorLZ4() {
    ZoneScoped;

    this->handle = LZ4_createStream();
    reset();
}

CompressorLZ4::~CompressorLZ4() {
    ZoneScoped;

    LZ4_freeStream(ls::bit_cast<LZ4_stream_t *>(this->handle));
}

std::vector<u8> CompressorLZ4::compress(void *src_data, usize src_size) {
    ZoneScoped;

    usize capacity = LZ4_COMPRESSBOUND(src_size);
    std::vector<u8> dst_data(capacity);

    usize compressed_size = LZ4_compress_fast_continue(
        ls::bit_cast<LZ4_stream_t *>(handle),
        reinterpret_cast<c8 *>(src_data),
        reinterpret_cast<c8 *>(dst_data.data()),
        static_cast<i32>(src_size),
        static_cast<i32>(capacity),
        1);
    dst_data.resize(compressed_size);

    return dst_data;
}

void CompressorLZ4::reset() {
    ZoneScoped;

    LZ4_initStream(this->handle, sizeof(LZ4_stream_t *));
}

CompressorZSTD::CompressorZSTD() {
    ZoneScoped;

    this->handle = ZSTD_createCStream();
    ZSTD_CCtx_setParameter(ls::bit_cast<ZSTD_CStream *>(this->handle), ZSTD_c_compressionLevel, 8);
    ZSTD_CCtx_setParameter(ls::bit_cast<ZSTD_CStream *>(this->handle), ZSTD_c_checksumFlag, 1);
    ZSTD_CCtx_setParameter(ls::bit_cast<ZSTD_CStream *>(this->handle), ZSTD_c_nbWorkers, 0);
}

CompressorZSTD::~CompressorZSTD() {
    ZoneScoped;

    ZSTD_freeCStream(ls::bit_cast<ZSTD_CStream *>(this->handle));
}

std::vector<u8> CompressorZSTD::compress(void *src_data, usize src_size) {
    ZoneScoped;

    usize capacity = ZSTD_compressBound(src_size);
    std::vector<u8> dst_data(capacity);

    ZSTD_inBuffer in_buffer = { .src = src_data, .size = src_size, .pos = 0 };
    ZSTD_outBuffer out_buffer = { .dst = dst_data.data(), .size = capacity, .pos = 0 };
    ZSTD_compressStream2(ls::bit_cast<ZSTD_CStream *>(this->handle), &out_buffer, &in_buffer, ZSTD_e_flush);
    dst_data.resize(out_buffer.pos);

    return dst_data;
}

void CompressorZSTD::reset() {
    ZoneScoped;

    ZSTD_CCtx_reset(ls::bit_cast<ZSTD_CStream *>(this->handle), ZSTD_reset_session_only);
}

std::vector<u8> CompressorNoop::compress([[maybe_unused]] void *src_data, [[maybe_unused]] usize src_size) {
    ZoneScoped;

    std::vector<u8> dst_data(src_size);
    std::memcpy(dst_data.data(), src_data, src_size);

    return dst_data;
}

}  // namespace lr
