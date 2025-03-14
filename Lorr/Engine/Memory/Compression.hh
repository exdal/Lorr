#pragma once

namespace lr {
struct CompressorI {
    virtual ~CompressorI() = default;
    virtual std::vector<u8> compress(void *src_data, usize src_size) = 0;
    virtual void reset() = 0;
};

struct CompressorLZ4 : CompressorI {
    CompressorLZ4();
    ~CompressorLZ4() override;
    std::vector<u8> compress(void *src_data, usize src_size) override;
    void reset() override;

    void *handle = nullptr;
};

struct CompressorZSTD : CompressorI {
    CompressorZSTD();
    ~CompressorZSTD() override;
    std::vector<u8> compress(void *src_data, usize src_size) override;
    void reset() override;

    void *handle = nullptr;
};

struct CompressorNoop : CompressorI {
    ~CompressorNoop() override = default;
    std::vector<u8> compress([[maybe_unused]] void *src_data, [[maybe_unused]] usize src_size) override;
    void reset() override {}
};

} // namespace lr
