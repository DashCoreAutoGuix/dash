// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <span.h>
#include <streams.h>

#include <array>

std::size_t CAutoFile::detail_fread(Span<std::byte> dst)
{
    if (!file) throw std::ios_base::failure("CAutoFile::read: file handle is nullptr");
    if (m_xor.empty()) {
        return std::fread(dst.data(), 1, dst.size(), file);
    } else {
        const auto init_pos{std::ftell(file)};
        if (init_pos < 0) throw std::ios_base::failure("CAutoFile::read: ftell failed");
        std::size_t ret{std::fread(dst.data(), 1, dst.size(), file)};
        util::Xor(dst.subspan(0, ret), m_xor, init_pos);
        return ret;
    }
}

void CAutoFile::read(Span<std::byte> dst)
{
    if (detail_fread(dst) != dst.size()) {
        throw std::ios_base::failure(feof() ? "CAutoFile::read: end of file" : "CAutoFile::read: fread failed");
    }
}

void CAutoFile::ignore(size_t nSize)
{
    if (!file) throw std::ios_base::failure("CAutoFile::ignore: file handle is nullptr");
    unsigned char data[4096];
    while (nSize > 0) {
        size_t nNow = std::min<size_t>(nSize, sizeof(data));
        if (std::fread(data, 1, nNow, file) != nNow) {
            throw std::ios_base::failure(feof() ? "CAutoFile::ignore: end of file" : "CAutoFile::ignore: fread failed");
        }
        nSize -= nNow;
    }
}

void CAutoFile::write(Span<const std::byte> src)
{
    if (!file) throw std::ios_base::failure("CAutoFile::write: file handle is nullptr");
    if (m_xor.empty()) {
        if (std::fwrite(src.data(), 1, src.size(), file) != src.size()) {
            throw std::ios_base::failure("CAutoFile::write: write failed");
        }
    } else {
        auto current_pos{std::ftell(file)};
        if (current_pos < 0) throw std::ios_base::failure("CAutoFile::write: ftell failed");
        std::array<std::byte, 4096> buf;
        while (src.size() > 0) {
            auto buf_now{Span{buf}.first(std::min<size_t>(src.size(), buf.size()))};
            std::copy(src.begin(), src.begin() + buf_now.size(), buf_now.begin());
            util::Xor(buf_now, m_xor, current_pos);
            if (std::fwrite(buf_now.data(), 1, buf_now.size(), file) != buf_now.size()) {
                throw std::ios_base::failure{"CAutoFile::write: failed"};
            }
            src = src.subspan(buf_now.size());
            current_pos += buf_now.size();
        }
    }
}
