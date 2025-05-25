#pragma once

#include <vector>
#include <cstdint>
#include <type_traits>
#include <stdexcept>
#include <string>

class BitStream
{
private:
    std::vector<std::uint8_t> buffer;
    size_t m_WritePose = 0;
    size_t m_ReadPose = 0;

public:
    BitStream();
    BitStream(const std::uint8_t* data, size_t size);

    void WriteBit(bool value);
    bool ReadBit();

    void WriteBits(uint32_t value, uint8_t bitCount);
    uint32_t ReadBits(uint8_t bitCount);

    void WriteBytes(const void* data, size_t size);
    void ReadBytes(void* data, size_t size);

    void AlignWrite();
    void AlignRead();

    template<typename T>
    void Write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        WriteBytes(&value, sizeof(T));
    }

    template<typename T>
    void Read(T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        ReadBytes(&value, sizeof(T));
    }

    void Write(const std::string& value);
    void Read(std::string& value);

    void WriteBoolArray(const std::vector<bool>& bools);
    std::vector<bool> ReadBoolArray();

    const std::uint8_t* GetData() const;
    size_t GetSizeBytes() const;
    size_t GetSizeBits() const;

    void ResetWrite();
    void ResetRead();
    void Clear();
};
