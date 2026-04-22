#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace LinkuraLocal::UnityAssetHelper {
    namespace Detail {
        enum class Endian {
            Big,
            Little
        };

        struct Reader {
            std::span<const uint8_t> bytes;
            size_t pos = 0;
            Endian endian = Endian::Big;

            [[nodiscard]] bool canRead(size_t n) const {
                return pos <= bytes.size() && n <= (bytes.size() - pos);
            }

            bool seek(size_t newPos) {
                if (newPos > bytes.size()) return false;
                pos = newPos;
                return true;
            }

            bool skip(size_t n) {
                if (!canRead(n)) return false;
                pos += n;
                return true;
            }

            bool align(size_t alignment) {
                if (alignment == 0) return false;
                const size_t mod = pos % alignment;
                if (mod == 0) return true;
                return skip(alignment - mod);
            }

            bool readU8(uint8_t& out) {
                if (!canRead(1)) return false;
                out = bytes[pos++];
                return true;
            }

            bool readU16(uint16_t& out) {
                if (!canRead(2)) return false;
                if (endian == Endian::Big) {
                    out = (static_cast<uint16_t>(bytes[pos]) << 8) |
                          static_cast<uint16_t>(bytes[pos + 1]);
                } else {
                    out = static_cast<uint16_t>(bytes[pos]) |
                          (static_cast<uint16_t>(bytes[pos + 1]) << 8);
                }
                pos += 2;
                return true;
            }

            bool readU32(uint32_t& out) {
                if (!canRead(4)) return false;
                if (endian == Endian::Big) {
                    out = (static_cast<uint32_t>(bytes[pos]) << 24) |
                          (static_cast<uint32_t>(bytes[pos + 1]) << 16) |
                          (static_cast<uint32_t>(bytes[pos + 2]) << 8) |
                          static_cast<uint32_t>(bytes[pos + 3]);
                } else {
                    out = static_cast<uint32_t>(bytes[pos]) |
                          (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
                          (static_cast<uint32_t>(bytes[pos + 2]) << 16) |
                          (static_cast<uint32_t>(bytes[pos + 3]) << 24);
                }
                pos += 4;
                return true;
            }

            bool readU64(uint64_t& out) {
                if (!canRead(8)) return false;
                if (endian == Endian::Big) {
                    out = (static_cast<uint64_t>(bytes[pos]) << 56) |
                          (static_cast<uint64_t>(bytes[pos + 1]) << 48) |
                          (static_cast<uint64_t>(bytes[pos + 2]) << 40) |
                          (static_cast<uint64_t>(bytes[pos + 3]) << 32) |
                          (static_cast<uint64_t>(bytes[pos + 4]) << 24) |
                          (static_cast<uint64_t>(bytes[pos + 5]) << 16) |
                          (static_cast<uint64_t>(bytes[pos + 6]) << 8) |
                          static_cast<uint64_t>(bytes[pos + 7]);
                } else {
                    out = static_cast<uint64_t>(bytes[pos]) |
                          (static_cast<uint64_t>(bytes[pos + 1]) << 8) |
                          (static_cast<uint64_t>(bytes[pos + 2]) << 16) |
                          (static_cast<uint64_t>(bytes[pos + 3]) << 24) |
                          (static_cast<uint64_t>(bytes[pos + 4]) << 32) |
                          (static_cast<uint64_t>(bytes[pos + 5]) << 40) |
                          (static_cast<uint64_t>(bytes[pos + 6]) << 48) |
                          (static_cast<uint64_t>(bytes[pos + 7]) << 56);
                }
                pos += 8;
                return true;
            }

            bool readCString(std::string& out) {
                out.clear();
                const size_t start = pos;
                while (pos < bytes.size() && bytes[pos] != 0) {
                    ++pos;
                }
                if (pos >= bytes.size()) return false;
                out.assign(reinterpret_cast<const char*>(bytes.data() + start), pos - start);
                ++pos;
                return true;
            }
        };

        inline size_t alignUp(size_t value, size_t alignment) {
            if (alignment == 0) return value;
            const size_t mod = value % alignment;
            return mod == 0 ? value : (value + alignment - mod);
        }

        inline void writeU16(std::vector<uint8_t>& out, uint16_t value, Endian endian) {
            if (endian == Endian::Big) {
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
            } else {
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
            }
        }

        inline void writeU32(std::vector<uint8_t>& out, uint32_t value, Endian endian) {
            if (endian == Endian::Big) {
                out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
            } else {
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
            }
        }

        inline void writeU64(std::vector<uint8_t>& out, uint64_t value, Endian endian) {
            if (endian == Endian::Big) {
                out.push_back(static_cast<uint8_t>((value >> 56) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 48) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 40) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 32) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
            } else {
                out.push_back(static_cast<uint8_t>(value & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 32) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 40) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 48) & 0xFFu));
                out.push_back(static_cast<uint8_t>((value >> 56) & 0xFFu));
            }
        }

        inline bool patchU32(std::span<uint8_t> bytes, size_t offset, uint32_t value, Endian endian) {
            if (offset + 4 > bytes.size()) return false;
            if (endian == Endian::Big) {
                bytes[offset] = static_cast<uint8_t>((value >> 24) & 0xFFu);
                bytes[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFFu);
                bytes[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFFu);
                bytes[offset + 3] = static_cast<uint8_t>(value & 0xFFu);
            } else {
                bytes[offset] = static_cast<uint8_t>(value & 0xFFu);
                bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
                bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
                bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
            }
            return true;
        }

        inline uint64_t fnv1a64(std::string_view s) {
            uint64_t hash = 1469598103934665603ull;
            for (const unsigned char c : s) {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        inline std::string hexU64(uint64_t value) {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string out(16, '0');
            for (int i = 15; i >= 0; --i) {
                out[i] = kHex[value & 0xFu];
                value >>= 4;
            }
            return out;
        }

        inline bool hasUnityFsSignature(std::span<const uint8_t> bytes, size_t offset) {
            static constexpr std::array<uint8_t, 8> kUnityFsSignature{
                    'U', 'n', 'i', 't', 'y', 'F', 'S', 0
            };
            return offset <= bytes.size() &&
                   kUnityFsSignature.size() <= (bytes.size() - offset) &&
                   std::memcmp(bytes.data() + offset,
                               kUnityFsSignature.data(),
                               kUnityFsSignature.size()) == 0;
        }

        inline bool resolveUnityFsOffset(std::span<const uint8_t> bytes, uint64_t requestedOffset, uint64_t& actualOffset) {
            if (requestedOffset <= bytes.size() && hasUnityFsSignature(bytes, static_cast<size_t>(requestedOffset))) {
                actualOffset = requestedOffset;
                return true;
            }
            if (requestedOffset == 0 &&
                bytes.size() >= 10 &&
                bytes[0] == 0xAB &&
                bytes[1] == 0x00 &&
                hasUnityFsSignature(bytes, 2)) {
                actualOffset = 2;
                return true;
            }
            return false;
        }

        inline bool readFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::string& error) {
            error.clear();
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) {
                error = "open failed";
                return false;
            }
            ifs.seekg(0, std::ios::end);
            const auto size = ifs.tellg();
            if (size < 0) {
                error = "tellg failed";
                return false;
            }
            out.resize(static_cast<size_t>(size));
            ifs.seekg(0, std::ios::beg);
            if (!out.empty()) {
                ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
                if (!ifs) {
                    error = "read failed";
                    return false;
                }
            }
            return true;
        }

        inline bool writeFileBytesAtomically(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error) {
            error.clear();
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec) {
                error = "create_directories failed";
                return false;
            }

            const auto tmpPath = path.string() + ".tmp";
            {
                std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
                if (!ofs) {
                    error = "open tmp output failed";
                    return false;
                }
                if (!bytes.empty()) {
                    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                }
                if (!ofs) {
                    error = "write tmp output failed";
                    return false;
                }
            }

            std::filesystem::rename(tmpPath, path, ec);
            if (ec) {
                std::filesystem::remove(path, ec);
                ec.clear();
                std::filesystem::rename(tmpPath, path, ec);
            }
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
                error = "rename failed";
                return false;
            }
            return true;
        }

        inline bool decompressLz4Block(std::span<const uint8_t> src, size_t expectedSize, std::vector<uint8_t>& dst, std::string& error) {
            error.clear();
            dst.clear();
            dst.reserve(expectedSize);

            size_t i = 0;
            while (i < src.size()) {
                const uint8_t token = src[i++];
                size_t literalLength = static_cast<size_t>(token >> 4);
                if (literalLength == 15) {
                    while (i < src.size()) {
                        const uint8_t extra = src[i++];
                        literalLength += extra;
                        if (extra != 255) break;
                    }
                }
                if (i + literalLength > src.size()) {
                    error = "lz4 literal overflow";
                    return false;
                }
                dst.insert(dst.end(), src.begin() + static_cast<std::ptrdiff_t>(i),
                           src.begin() + static_cast<std::ptrdiff_t>(i + literalLength));
                i += literalLength;
                if (i >= src.size()) break;

                if (i + 2 > src.size()) {
                    error = "lz4 missing match offset";
                    return false;
                }
                const size_t matchOffset = static_cast<size_t>(src[i]) |
                                           (static_cast<size_t>(src[i + 1]) << 8);
                i += 2;
                if (matchOffset == 0 || matchOffset > dst.size()) {
                    error = "lz4 invalid match offset";
                    return false;
                }

                size_t matchLength = static_cast<size_t>(token & 0x0F) + 4;
                if ((token & 0x0F) == 15) {
                    while (i < src.size()) {
                        const uint8_t extra = src[i++];
                        matchLength += extra;
                        if (extra != 255) break;
                    }
                }

                const size_t matchStart = dst.size() - matchOffset;
                dst.resize(dst.size() + matchLength);
                for (size_t j = 0; j < matchLength; ++j) {
                    dst[dst.size() - matchLength + j] = dst[matchStart + j];
                }
            }

            if (dst.size() != expectedSize) {
                error = "lz4 size mismatch";
                return false;
            }
            return true;
        }

        struct SerializedTargetField {
            size_t offset = 0;
            uint32_t currentValue = 0;
            Endian endian = Endian::Little;
        };

        inline std::optional<SerializedTargetField> findSerializedFileTargetField(std::span<const uint8_t> bytes) {
            if (bytes.size() < 20) return std::nullopt;

            Reader reader{bytes};
            uint32_t metadataSize = 0;
            uint32_t fileSize32 = 0;
            uint32_t version = 0;
            uint32_t dataOffset32 = 0;
            if (!reader.readU32(metadataSize) ||
                !reader.readU32(fileSize32) ||
                !reader.readU32(version) ||
                !reader.readU32(dataOffset32)) {
                return std::nullopt;
            }

            uint64_t fileSize = fileSize32;
            uint8_t fileEndian = 0;
            if (version >= 9) {
                if (!reader.readU8(fileEndian) || !reader.skip(3)) {
                    return std::nullopt;
                }
            } else {
                if (fileSize < metadataSize || fileSize > bytes.size()) {
                    return std::nullopt;
                }
                fileEndian = bytes[static_cast<size_t>(fileSize - metadataSize)];
            }

            if (version >= 22) {
                uint64_t dataOffset64 = 0;
                if (!reader.readU32(metadataSize) ||
                    !reader.readU64(fileSize) ||
                    !reader.readU64(dataOffset64) ||
                    !reader.skip(8)) {
                    return std::nullopt;
                }
            }

            if (fileSize == 0 || fileSize > bytes.size()) {
                return std::nullopt;
            }

            reader.endian = fileEndian == 0 ? Endian::Little : Endian::Big;

            if (version >= 7) {
                std::string unityVersion;
                if (!reader.readCString(unityVersion)) {
                    return std::nullopt;
                }
            }

            if (version < 8 || !reader.canRead(4)) {
                return std::nullopt;
            }

            const size_t targetOffset = reader.pos;
            uint32_t targetValue = 0;
            if (!reader.readU32(targetValue)) {
                return std::nullopt;
            }

            return SerializedTargetField{
                    .offset = targetOffset,
                    .currentValue = targetValue,
                    .endian = reader.endian,
            };
        }

        struct StorageBlock {
            uint32_t uncompressedSize = 0;
            uint32_t compressedSize = 0;
            uint16_t flags = 0;
        };

        struct Node {
            uint64_t offset = 0;
            uint64_t size = 0;
            uint32_t flags = 0;
            std::string path;
        };

        struct BundleHeader {
            std::string signature;
            uint32_t version = 0;
            std::string unityVersion;
            std::string unityRevision;
            uint64_t size = 0;
            uint32_t compressedBlocksInfoSize = 0;
            uint32_t uncompressedBlocksInfoSize = 0;
            uint32_t flags = 0;
            size_t alignedHeaderSize = 0;
            size_t blockDataStart = 0;
        };

        struct ParsedBundle {
            BundleHeader header;
            std::vector<StorageBlock> blocks;
            std::vector<Node> nodes;
            std::vector<uint8_t> rawData;
        };

        inline bool parseUnityFsBundle(std::span<const uint8_t> bytes, ParsedBundle& out, std::string& error) {
            error.clear();
            out = {};

            Reader reader{bytes};
            if (!reader.readCString(out.header.signature) || out.header.signature != "UnityFS") {
                error = "not a UnityFS stream";
                return false;
            }
            if (!reader.readU32(out.header.version) ||
                !reader.readCString(out.header.unityVersion) ||
                !reader.readCString(out.header.unityRevision) ||
                !reader.readU64(out.header.size) ||
                !reader.readU32(out.header.compressedBlocksInfoSize) ||
                !reader.readU32(out.header.uncompressedBlocksInfoSize) ||
                !reader.readU32(out.header.flags)) {
                error = "failed to read UnityFS header";
                return false;
            }

            if (out.header.version >= 7 && !reader.align(16)) {
                error = "failed to align UnityFS header";
                return false;
            }
            out.header.alignedHeaderSize = reader.pos;

            const bool blocksInfoAtEnd = (out.header.flags & 0x80u) != 0;
            size_t blocksInfoPos = reader.pos;
            if (blocksInfoAtEnd) {
                if (out.header.compressedBlocksInfoSize > bytes.size()) {
                    error = "compressed blocks info is larger than stream";
                    return false;
                }
                blocksInfoPos = bytes.size() - out.header.compressedBlocksInfoSize;
            }

            if (blocksInfoPos + out.header.compressedBlocksInfoSize > bytes.size()) {
                error = "blocks info range is out of bounds";
                return false;
            }

            const auto blocksInfoCompressed = bytes.subspan(blocksInfoPos, out.header.compressedBlocksInfoSize);
            std::vector<uint8_t> blocksInfoStorage;
            const uint32_t blocksInfoCompression = out.header.flags & 0x3Fu;
            if (blocksInfoCompression == 0) {
                blocksInfoStorage.assign(blocksInfoCompressed.begin(), blocksInfoCompressed.end());
            } else if (blocksInfoCompression == 2 || blocksInfoCompression == 3) {
                if (!decompressLz4Block(blocksInfoCompressed, out.header.uncompressedBlocksInfoSize, blocksInfoStorage, error)) {
                    error = "blocks info " + error;
                    return false;
                }
            } else {
                error = "unsupported UnityFS blocks info compression";
                return false;
            }

            Reader blocksReader{std::span<const uint8_t>(blocksInfoStorage.data(), blocksInfoStorage.size())};
            if (!blocksReader.skip(16)) {
                error = "blocks info hash is truncated";
                return false;
            }

            uint32_t blockCount = 0;
            if (!blocksReader.readU32(blockCount)) {
                error = "failed to read UnityFS block count";
                return false;
            }

            uint64_t totalRawSize = 0;
            out.blocks.reserve(blockCount);
            for (uint32_t i = 0; i < blockCount; ++i) {
                StorageBlock block;
                if (!blocksReader.readU32(block.uncompressedSize) ||
                    !blocksReader.readU32(block.compressedSize) ||
                    !blocksReader.readU16(block.flags)) {
                    error = "failed to read UnityFS block table";
                    return false;
                }
                totalRawSize += block.uncompressedSize;
                if (totalRawSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
                    error = "bundle raw size is too large";
                    return false;
                }
                out.blocks.push_back(block);
            }

            uint32_t nodeCount = 0;
            if (!blocksReader.readU32(nodeCount)) {
                error = "failed to read UnityFS node count";
                return false;
            }

            out.nodes.reserve(nodeCount);
            for (uint32_t i = 0; i < nodeCount; ++i) {
                Node node;
                if (!blocksReader.readU64(node.offset) ||
                    !blocksReader.readU64(node.size) ||
                    !blocksReader.readU32(node.flags) ||
                    !blocksReader.readCString(node.path)) {
                    error = "failed to read UnityFS node table";
                    return false;
                }
                out.nodes.push_back(std::move(node));
            }

            out.header.blockDataStart = reader.pos;
            if (!blocksInfoAtEnd) {
                out.header.blockDataStart += out.header.compressedBlocksInfoSize;
            }
            if ((out.header.flags & 0x200u) != 0) {
                out.header.blockDataStart = alignUp(out.header.blockDataStart, 16);
            }

            if (out.header.blockDataStart > bytes.size()) {
                error = "UnityFS block data start is out of bounds";
                return false;
            }

            out.rawData.clear();
            out.rawData.reserve(static_cast<size_t>(totalRawSize));

            size_t blockCursor = out.header.blockDataStart;
            for (const auto& block : out.blocks) {
                if (blockCursor + block.compressedSize > bytes.size()) {
                    error = "UnityFS block range is out of bounds";
                    return false;
                }
                const auto compressedBlock = bytes.subspan(blockCursor, block.compressedSize);
                blockCursor += block.compressedSize;

                const uint16_t blockCompression = block.flags & 0x3Fu;
                if (blockCompression == 0) {
                    if (block.uncompressedSize != block.compressedSize) {
                        error = "uncompressed block size mismatch";
                        return false;
                    }
                    out.rawData.insert(out.rawData.end(), compressedBlock.begin(), compressedBlock.end());
                } else if (blockCompression == 2 || blockCompression == 3) {
                    std::vector<uint8_t> rawBlock;
                    if (!decompressLz4Block(compressedBlock, block.uncompressedSize, rawBlock, error)) {
                        error = "bundle block " + error;
                        return false;
                    }
                    out.rawData.insert(out.rawData.end(), rawBlock.begin(), rawBlock.end());
                } else {
                    error = "unsupported UnityFS data block compression";
                    return false;
                }
            }

            return true;
        }

        inline bool rebuildUnityFsBundle(std::span<const uint8_t> prefix, const ParsedBundle& bundle, std::vector<uint8_t>& out, std::string& error) {
            error.clear();
            out.clear();

            std::vector<uint8_t> blocksInfo;
            blocksInfo.resize(16, 0);
            writeU32(blocksInfo, static_cast<uint32_t>(bundle.blocks.size()), Endian::Big);

            size_t rawOffset = 0;
            for (const auto& block : bundle.blocks) {
                const uint64_t blockEnd = rawOffset + static_cast<uint64_t>(block.uncompressedSize);
                if (blockEnd > bundle.rawData.size()) {
                    error = "raw data is shorter than block table";
                    return false;
                }
                const uint16_t rebuiltBlockFlags = static_cast<uint16_t>(block.flags & ~0x3Fu);
                writeU32(blocksInfo, block.uncompressedSize, Endian::Big);
                writeU32(blocksInfo, block.uncompressedSize, Endian::Big);
                writeU16(blocksInfo, rebuiltBlockFlags, Endian::Big);
                rawOffset += block.uncompressedSize;
            }
            if (rawOffset != bundle.rawData.size()) {
                error = "raw data size does not match block table";
                return false;
            }

            writeU32(blocksInfo, static_cast<uint32_t>(bundle.nodes.size()), Endian::Big);
            for (const auto& node : bundle.nodes) {
                if (node.offset + node.size > bundle.rawData.size()) {
                    error = "node range is out of bounds";
                    return false;
                }
                writeU64(blocksInfo, node.offset, Endian::Big);
                writeU64(blocksInfo, node.size, Endian::Big);
                writeU32(blocksInfo, node.flags, Endian::Big);
                blocksInfo.insert(blocksInfo.end(), node.path.begin(), node.path.end());
                blocksInfo.push_back(0);
            }

            uint32_t outFlags = bundle.header.flags & ~0x3Fu;
            if ((outFlags & 0xC0u) == 0u) {
                outFlags |= 0x40u;
            }
            const bool blocksInfoAtEnd = (outFlags & 0x80u) != 0u;

            out.insert(out.end(), prefix.begin(), prefix.end());
            const size_t streamStart = out.size();

            out.insert(out.end(), bundle.header.signature.begin(), bundle.header.signature.end());
            out.push_back(0);
            writeU32(out, bundle.header.version, Endian::Big);
            out.insert(out.end(), bundle.header.unityVersion.begin(), bundle.header.unityVersion.end());
            out.push_back(0);
            out.insert(out.end(), bundle.header.unityRevision.begin(), bundle.header.unityRevision.end());
            out.push_back(0);

            const size_t sizeFieldPos = out.size();
            writeU64(out, 0, Endian::Big);
            writeU32(out, static_cast<uint32_t>(blocksInfo.size()), Endian::Big);
            writeU32(out, static_cast<uint32_t>(blocksInfo.size()), Endian::Big);
            writeU32(out, outFlags, Endian::Big);

            if (bundle.header.version >= 7) {
                while (((out.size() - streamStart) % 16u) != 0u) {
                    out.push_back(0);
                }
            }

            if (!blocksInfoAtEnd) {
                out.insert(out.end(), blocksInfo.begin(), blocksInfo.end());
            }

            if ((outFlags & 0x200u) != 0u) {
                while (((out.size() - streamStart) % 16u) != 0u) {
                    out.push_back(0);
                }
            }

            rawOffset = 0;
            for (const auto& block : bundle.blocks) {
                const size_t len = block.uncompressedSize;
                out.insert(out.end(),
                           bundle.rawData.begin() + static_cast<std::ptrdiff_t>(rawOffset),
                           bundle.rawData.begin() + static_cast<std::ptrdiff_t>(rawOffset + len));
                rawOffset += len;
            }

            if (blocksInfoAtEnd) {
                out.insert(out.end(), blocksInfo.begin(), blocksInfo.end());
            }

            const uint64_t bundleSize = out.size() - streamStart;
            if (sizeFieldPos + 8 > out.size()) {
                error = "invalid size field position";
                return false;
            }
            out[sizeFieldPos + 0] = static_cast<uint8_t>((bundleSize >> 56) & 0xFFu);
            out[sizeFieldPos + 1] = static_cast<uint8_t>((bundleSize >> 48) & 0xFFu);
            out[sizeFieldPos + 2] = static_cast<uint8_t>((bundleSize >> 40) & 0xFFu);
            out[sizeFieldPos + 3] = static_cast<uint8_t>((bundleSize >> 32) & 0xFFu);
            out[sizeFieldPos + 4] = static_cast<uint8_t>((bundleSize >> 24) & 0xFFu);
            out[sizeFieldPos + 5] = static_cast<uint8_t>((bundleSize >> 16) & 0xFFu);
            out[sizeFieldPos + 6] = static_cast<uint8_t>((bundleSize >> 8) & 0xFFu);
            out[sizeFieldPos + 7] = static_cast<uint8_t>(bundleSize & 0xFFu);
            return true;
        }

        inline std::filesystem::path makePatchedOutputPath(const std::filesystem::path& sourcePath,
                                                           uint64_t loadOffset,
                                                           uint32_t desiredBuildTarget,
                                                           uint64_t fileSize,
                                                           long long lastWriteTicks) {
            std::string key = sourcePath.string();
            key.push_back('|');
            key += std::to_string(loadOffset);
            key.push_back('|');
            key += std::to_string(desiredBuildTarget);
            key.push_back('|');
            key += std::to_string(fileSize);
            key.push_back('|');
            key += std::to_string(lastWriteTicks);

            const auto outputDir = sourcePath.parent_path() / ".linkura_localify_patched";
            const auto stem = sourcePath.filename().string();
            const auto hash = hexU64(fnv1a64(key));
            return outputDir / (stem + "." + hash);
        }
    } // namespace Detail

    enum : uint32_t {
        kBuildTargetIos = 0x09,
        kBuildTargetAndroid = 0x0D,
    };

    struct PatchLoadResult {
        std::string loadPath;
        uint64_t loadOffset = 0;
        bool usedPatchedCopy = false;
        bool targetAlreadyMatched = false;
        bool foundSerializedFile = false;
        size_t patchedSerializedFileCount = 0;
        uint32_t firstObservedBuildTarget = 0;
        uint64_t actualOffset = 0;
        uint64_t firstNodeOffset = 0;
        uint64_t firstNodeSize = 0;
        uint64_t firstTargetOffsetInNode = 0;
        uint64_t firstTargetAbsoluteRawOffset = 0;
        int64_t firstBlockIndex = -1;
        uint32_t firstBlockCompression = 0;
        bool firstDirectFileOffsetValid = false;
        uint64_t firstDirectFileOffset = 0;
        std::string error;
    };

    struct PatchFileResult {
        bool patched = false;
        bool targetAlreadyMatched = false;
        bool foundSerializedFile = false;
        size_t patchedSerializedFileCount = 0;
        uint32_t firstObservedBuildTarget = 0;
        uint64_t actualOffset = 0;
        uint64_t firstNodeOffset = 0;
        uint64_t firstNodeSize = 0;
        uint64_t firstTargetOffsetInNode = 0;
        uint64_t firstTargetAbsoluteRawOffset = 0;
        int64_t firstBlockIndex = -1;
        uint32_t firstBlockCompression = 0;
        bool firstDirectFileOffsetValid = false;
        uint64_t firstDirectFileOffset = 0;
        std::string error;
    };

    struct MemoryPatchField {
        size_t offsetInBlock = 0;
        uint32_t originalValue = 0;
        uint32_t patchedValue = 0;
        Detail::Endian endian = Detail::Endian::Little;
    };

    struct MemoryPatchBlock {
        size_t blockIndex = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        std::array<uint8_t, 16> compressedPrefix{};
        size_t compressedPrefixSize = 0;
        std::vector<MemoryPatchField> fields;
    };

    struct MemoryPatchPlan {
        bool foundSerializedFile = false;
        bool targetAlreadyMatched = false;
        size_t patchedSerializedFileCount = 0;
        uint32_t firstObservedBuildTarget = 0;
        uint64_t actualOffset = 0;
        std::vector<MemoryPatchBlock> blocks;
        std::string error;
    };

    inline MemoryPatchPlan BuildMemoryPatchPlanForLz4Blocks(const std::string& sourcePath,
                                                            uint64_t requestedOffset,
                                                            uint32_t desiredBuildTarget) {
        MemoryPatchPlan result;
        if (sourcePath.empty()) {
            result.error = "source path is empty";
            return result;
        }

        const std::filesystem::path path(sourcePath);
        std::vector<uint8_t> fileBytes;
        if (!Detail::readFileBytes(path, fileBytes, result.error)) {
            return result;
        }

        const auto tryBuildAtOffset = [&](uint64_t actualOffset, MemoryPatchPlan& state) -> bool {
            if (actualOffset > fileBytes.size()) {
                state.error = "load offset is out of bounds";
                return false;
            }

            uint64_t bundleOffset = 0;
            if (!Detail::resolveUnityFsOffset(std::span<const uint8_t>(fileBytes.data(), fileBytes.size()),
                                              actualOffset,
                                              bundleOffset)) {
                state.error = "not a UnityFS/LZ4 bundle";
                return false;
            }

            const auto stream = std::span<const uint8_t>(fileBytes.data() + static_cast<size_t>(bundleOffset),
                                                         fileBytes.size() - static_cast<size_t>(bundleOffset));
            Detail::ParsedBundle bundle;
            std::string parseError;
            if (!Detail::parseUnityFsBundle(stream, bundle, parseError)) {
                state.error = parseError.empty() ? "not a UnityFS/LZ4 bundle" : parseError;
                return false;
            }

            size_t compressedCursor = bundle.header.blockDataStart;
            std::vector<size_t> rawBlockStarts;
            rawBlockStarts.reserve(bundle.blocks.size());
            size_t rawCursor = 0;
            for (size_t i = 0; i < bundle.blocks.size(); ++i) {
                const auto& block = bundle.blocks[i];
                rawBlockStarts.push_back(rawCursor);
                rawCursor += block.uncompressedSize;

                if (compressedCursor + block.compressedSize > stream.size()) {
                    state.error = "compressed block range is out of bounds";
                    return false;
                }
                compressedCursor += block.compressedSize;
            }

            bool firstObservedSet = false;
            for (const auto& node : bundle.nodes) {
                if (node.offset + node.size > bundle.rawData.size()) continue;
                auto nodeSpan = std::span<const uint8_t>(bundle.rawData.data() + static_cast<size_t>(node.offset),
                                                        static_cast<size_t>(node.size));
                const auto field = Detail::findSerializedFileTargetField(nodeSpan);
                if (!field) continue;

                state.foundSerializedFile = true;
                if (!firstObservedSet) {
                    firstObservedSet = true;
                    state.firstObservedBuildTarget = field->currentValue;
                }
                if (field->currentValue == desiredBuildTarget) continue;

                const size_t absoluteRawOffset = static_cast<size_t>(node.offset) + field->offset;
                size_t blockIndex = 0;
                bool foundBlock = false;
                for (; blockIndex < bundle.blocks.size(); ++blockIndex) {
                    const size_t blockStart = rawBlockStarts[blockIndex];
                    const size_t blockEnd = blockStart + bundle.blocks[blockIndex].uncompressedSize;
                    if (absoluteRawOffset + 4 <= blockEnd) {
                        foundBlock = true;
                        break;
                    }
                }
                if (!foundBlock) {
                    state.error = "target field is not inside any raw block";
                    return false;
                }

                const auto& block = bundle.blocks[blockIndex];
                const uint16_t compression = block.flags & 0x3Fu;
                if (compression != 2 && compression != 3) {
                    state.error = "target field is in a non-LZ4 block";
                    return false;
                }

                auto& patchBlock = [&]() -> MemoryPatchBlock& {
                    for (auto& existing : state.blocks) {
                        if (existing.blockIndex == blockIndex) return existing;
                    }
                    MemoryPatchBlock created;
                    created.blockIndex = blockIndex;
                    created.compressedSize = block.compressedSize;
                    created.uncompressedSize = block.uncompressedSize;

                    size_t tmpCompressedCursor = bundle.header.blockDataStart;
                    for (size_t i = 0; i < blockIndex; ++i) {
                        tmpCompressedCursor += bundle.blocks[i].compressedSize;
                    }
                    created.compressedPrefixSize = std::min<size_t>(created.compressedPrefix.size(), block.compressedSize);
                    if (created.compressedPrefixSize > 0) {
                        std::memcpy(created.compressedPrefix.data(),
                                    stream.data() + static_cast<std::ptrdiff_t>(tmpCompressedCursor),
                                    created.compressedPrefixSize);
                    }

                    state.blocks.push_back(created);
                    return state.blocks.back();
                }();

                patchBlock.fields.push_back(MemoryPatchField{
                        .offsetInBlock = absoluteRawOffset - rawBlockStarts[blockIndex],
                        .originalValue = field->currentValue,
                        .patchedValue = desiredBuildTarget,
                        .endian = field->endian,
                });
                ++state.patchedSerializedFileCount;
            }

            state.actualOffset = bundleOffset;
            state.targetAlreadyMatched = state.foundSerializedFile && state.patchedSerializedFileCount == 0;
            if (!state.foundSerializedFile) {
                state.error = "no serialized file found inside UnityFS bundle";
                return false;
            }
            return true;
        };

        if (tryBuildAtOffset(requestedOffset, result)) {
            return result;
        }

        if (result.error.empty()) {
            result.error = "failed to build memory patch plan";
        }
        return result;
    }

    inline PatchFileResult PatchFileTargetBuildIdInPlace(const std::string& sourcePath,
                                                         uint64_t requestedOffset,
                                                         uint32_t desiredBuildTarget) {
        PatchFileResult result;
        if (sourcePath.empty()) {
            result.error = "source path is empty";
            return result;
        }

        const std::filesystem::path path(sourcePath);
        std::vector<uint8_t> fileBytes;
        if (!Detail::readFileBytes(path, fileBytes, result.error)) {
            return result;
        }

        const auto tryPatchAtOffset = [&](uint64_t actualOffset, PatchFileResult& state) -> bool {
            if (actualOffset > fileBytes.size()) {
                state.error = "load offset is out of bounds";
                return false;
            }

            auto stream = std::span<const uint8_t>(fileBytes.data() + static_cast<size_t>(actualOffset),
                                                   fileBytes.size() - static_cast<size_t>(actualOffset));
            if (stream.empty()) {
                state.error = "load stream is empty";
                return false;
            }

            std::vector<uint8_t> patchedBytes;
            std::string parseError;

            Detail::ParsedBundle bundle;
            uint64_t bundleOffset = 0;
            const bool isUnityFs = Detail::resolveUnityFsOffset(std::span<const uint8_t>(fileBytes.data(), fileBytes.size()),
                                                                actualOffset,
                                                                bundleOffset);
            const auto bundleStream = isUnityFs
                                      ? std::span<const uint8_t>(fileBytes.data() + static_cast<size_t>(bundleOffset),
                                                                 fileBytes.size() - static_cast<size_t>(bundleOffset))
                                      : std::span<const uint8_t>{};
            if (isUnityFs && Detail::parseUnityFsBundle(bundleStream, bundle, parseError)) {
                state.actualOffset = bundleOffset;
                bool firstObservedSet = false;
                std::vector<size_t> rawBlockStarts;
                rawBlockStarts.reserve(bundle.blocks.size());
                size_t rawCursor = 0;
                for (const auto& block : bundle.blocks) {
                    rawBlockStarts.push_back(rawCursor);
                    rawCursor += block.uncompressedSize;
                }
                for (const auto& node : bundle.nodes) {
                    if (node.offset + node.size > bundle.rawData.size()) continue;
                    auto nodeSpan = std::span<const uint8_t>(bundle.rawData.data() + static_cast<size_t>(node.offset),
                                                            static_cast<size_t>(node.size));
                    const auto field = Detail::findSerializedFileTargetField(nodeSpan);
                    if (!field) continue;

                    state.foundSerializedFile = true;
                    if (!firstObservedSet) {
                        firstObservedSet = true;
                        state.firstObservedBuildTarget = field->currentValue;
                        state.firstNodeOffset = node.offset;
                        state.firstNodeSize = node.size;
                        state.firstTargetOffsetInNode = field->offset;
                        state.firstTargetAbsoluteRawOffset = node.offset + field->offset;
                        for (size_t i = 0; i < bundle.blocks.size(); ++i) {
                            const size_t blockStart = rawBlockStarts[i];
                            const size_t blockEnd = blockStart + bundle.blocks[i].uncompressedSize;
                            if (state.firstTargetAbsoluteRawOffset + 4 <= blockEnd) {
                                state.firstBlockIndex = static_cast<int64_t>(i);
                                state.firstBlockCompression = bundle.blocks[i].flags & 0x3Fu;
                                if (state.firstBlockCompression == 0) {
                                    size_t compressedBlockStart = bundle.header.blockDataStart;
                                    for (size_t j = 0; j < i; ++j) {
                                        compressedBlockStart += bundle.blocks[j].compressedSize;
                                    }
                                    state.firstDirectFileOffsetValid = true;
                                    state.firstDirectFileOffset = bundleOffset +
                                                                  compressedBlockStart +
                                                                  (state.firstTargetAbsoluteRawOffset - blockStart);
                                }
                                break;
                            }
                        }
                    }
                    if (field->currentValue == desiredBuildTarget) continue;

                    auto mutableSpan = std::span<uint8_t>(bundle.rawData.data(), bundle.rawData.size());
                    if (!Detail::patchU32(mutableSpan,
                                          static_cast<size_t>(node.offset) + field->offset,
                                          desiredBuildTarget,
                                          field->endian)) {
                        state.error = "failed to patch serialized file target";
                        return false;
                    }
                    ++state.patchedSerializedFileCount;
                }

                state.targetAlreadyMatched = state.foundSerializedFile && state.patchedSerializedFileCount == 0;
                if (state.patchedSerializedFileCount == 0) {
                    if (!state.foundSerializedFile) {
                        state.error = "no serialized file found inside UnityFS bundle";
                    }
                    return state.foundSerializedFile;
                }

                if (!Detail::rebuildUnityFsBundle(std::span<const uint8_t>(fileBytes.data(), static_cast<size_t>(bundleOffset)),
                                                  bundle,
                                                  patchedBytes,
                                                  state.error)) {
                    return false;
                }
            } else {
                const auto field = Detail::findSerializedFileTargetField(stream);
                if (!field) {
                    state.error = parseError.empty() ? "unsupported asset format" : parseError;
                    return false;
                }

                state.actualOffset = actualOffset;
                state.foundSerializedFile = true;
                state.firstObservedBuildTarget = field->currentValue;
                state.firstTargetOffsetInNode = field->offset;
                state.firstDirectFileOffsetValid = true;
                state.firstDirectFileOffset = actualOffset + field->offset;
                state.targetAlreadyMatched = field->currentValue == desiredBuildTarget;
                if (state.targetAlreadyMatched) {
                    return true;
                }

                patchedBytes = fileBytes;
                if (!Detail::patchU32(std::span<uint8_t>(patchedBytes.data() + static_cast<size_t>(actualOffset),
                                                         patchedBytes.size() - static_cast<size_t>(actualOffset)),
                                      field->offset,
                                      desiredBuildTarget,
                                      field->endian)) {
                    state.error = "failed to patch direct serialized file target";
                    return false;
                }
                state.patchedSerializedFileCount = 1;
            }

            if (state.patchedSerializedFileCount == 0) {
                return true;
            }

            if (!Detail::writeFileBytesAtomically(path, patchedBytes, state.error)) {
                return false;
            }
            state.patched = true;
            return true;
        };

        if (tryPatchAtOffset(requestedOffset, result)) {
            return result;
        }

        if (result.error.empty()) {
            result.error = "failed to patch source file";
        }
        return result;
    }

    inline PatchLoadResult PreparePatchedLoadTarget(const std::string& sourcePath,
                                                    uint64_t requestedOffset,
                                                    uint32_t desiredBuildTarget) {
        PatchLoadResult result{
                .loadPath = sourcePath,
                .loadOffset = requestedOffset,
        };
        if (sourcePath.empty()) {
            result.error = "source path is empty";
            return result;
        }

        const std::filesystem::path path(sourcePath);
        std::error_code ec;
        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec) {
            result.error = "file_size failed";
            return result;
        }
        const auto lastWrite = std::filesystem::last_write_time(path, ec);
        if (ec) {
            result.error = "last_write_time failed";
            return result;
        }

        std::vector<uint8_t> fileBytes;
        if (!Detail::readFileBytes(path, fileBytes, result.error)) {
            return result;
        }

        const auto tryPatchAtOffset = [&](uint64_t actualOffset, PatchLoadResult& state) -> bool {
            if (actualOffset > fileBytes.size()) {
                state.error = "load offset is out of bounds";
                return false;
            }

            const auto stream = std::span<const uint8_t>(fileBytes.data() + static_cast<size_t>(actualOffset),
                                                         fileBytes.size() - static_cast<size_t>(actualOffset));
            if (stream.empty()) {
                state.error = "load stream is empty";
                return false;
            }

            std::vector<uint8_t> patchedBytes;
            std::string parseError;

            Detail::ParsedBundle bundle;
            uint64_t bundleOffset = 0;
            const bool isUnityFs = Detail::resolveUnityFsOffset(std::span<const uint8_t>(fileBytes.data(), fileBytes.size()),
                                                                actualOffset,
                                                                bundleOffset);
            const auto bundleStream = isUnityFs
                                      ? std::span<const uint8_t>(fileBytes.data() + static_cast<size_t>(bundleOffset),
                                                                 fileBytes.size() - static_cast<size_t>(bundleOffset))
                                      : std::span<const uint8_t>{};
            if (isUnityFs && Detail::parseUnityFsBundle(bundleStream, bundle, parseError)) {
                state.loadOffset = bundleOffset;
                state.actualOffset = bundleOffset;
                bool firstObservedSet = false;
                std::vector<size_t> rawBlockStarts;
                rawBlockStarts.reserve(bundle.blocks.size());
                size_t rawCursor = 0;
                for (const auto& block : bundle.blocks) {
                    rawBlockStarts.push_back(rawCursor);
                    rawCursor += block.uncompressedSize;
                }
                for (const auto& node : bundle.nodes) {
                    if (node.offset + node.size > bundle.rawData.size()) continue;
                    auto nodeSpan = std::span<const uint8_t>(bundle.rawData.data() + static_cast<size_t>(node.offset),
                                                            static_cast<size_t>(node.size));
                    const auto field = Detail::findSerializedFileTargetField(nodeSpan);
                    if (!field) continue;

                    state.foundSerializedFile = true;
                    if (!firstObservedSet) {
                        firstObservedSet = true;
                        state.firstObservedBuildTarget = field->currentValue;
                        state.firstNodeOffset = node.offset;
                        state.firstNodeSize = node.size;
                        state.firstTargetOffsetInNode = field->offset;
                        state.firstTargetAbsoluteRawOffset = node.offset + field->offset;
                        for (size_t i = 0; i < bundle.blocks.size(); ++i) {
                            const size_t blockStart = rawBlockStarts[i];
                            const size_t blockEnd = blockStart + bundle.blocks[i].uncompressedSize;
                            if (state.firstTargetAbsoluteRawOffset + 4 <= blockEnd) {
                                state.firstBlockIndex = static_cast<int64_t>(i);
                                state.firstBlockCompression = bundle.blocks[i].flags & 0x3Fu;
                                if (state.firstBlockCompression == 0) {
                                    size_t compressedBlockStart = bundle.header.blockDataStart;
                                    for (size_t j = 0; j < i; ++j) {
                                        compressedBlockStart += bundle.blocks[j].compressedSize;
                                    }
                                    state.firstDirectFileOffsetValid = true;
                                    state.firstDirectFileOffset = bundleOffset +
                                                                  compressedBlockStart +
                                                                  (state.firstTargetAbsoluteRawOffset - blockStart);
                                }
                                break;
                            }
                        }
                    }
                    if (field->currentValue == desiredBuildTarget) continue;

                    auto mutableSpan = std::span<uint8_t>(bundle.rawData.data(), bundle.rawData.size());
                    if (!Detail::patchU32(mutableSpan,
                                          static_cast<size_t>(node.offset) + field->offset,
                                          desiredBuildTarget,
                                          field->endian)) {
                        state.error = "failed to patch serialized file target";
                        return false;
                    }
                    ++state.patchedSerializedFileCount;
                }

                state.targetAlreadyMatched = state.foundSerializedFile && state.patchedSerializedFileCount == 0;
                if (state.patchedSerializedFileCount == 0) {
                    if (!state.foundSerializedFile) {
                        state.error = "no serialized file found inside UnityFS bundle";
                    }
                    return state.foundSerializedFile;
                }

                if (!Detail::rebuildUnityFsBundle(std::span<const uint8_t>(fileBytes.data(), static_cast<size_t>(bundleOffset)),
                                                  bundle,
                                                  patchedBytes,
                                                  state.error)) {
                    return false;
                }
            } else {
                const auto field = Detail::findSerializedFileTargetField(stream);
                if (!field) {
                    state.error = parseError.empty() ? "unsupported asset format" : parseError;
                    return false;
                }

                state.actualOffset = actualOffset;
                state.foundSerializedFile = true;
                state.firstObservedBuildTarget = field->currentValue;
                state.firstTargetOffsetInNode = field->offset;
                state.firstDirectFileOffsetValid = true;
                state.firstDirectFileOffset = actualOffset + field->offset;
                state.targetAlreadyMatched = field->currentValue == desiredBuildTarget;
                if (state.targetAlreadyMatched) {
                    return true;
                }

                patchedBytes = fileBytes;
                if (!Detail::patchU32(std::span<uint8_t>(patchedBytes.data() + static_cast<size_t>(actualOffset),
                                                         patchedBytes.size() - static_cast<size_t>(actualOffset)),
                                      field->offset,
                                      desiredBuildTarget,
                                      field->endian)) {
                    state.error = "failed to patch direct serialized file target";
                    return false;
                }
                state.patchedSerializedFileCount = 1;
            }

            if (state.patchedSerializedFileCount == 0) {
                return true;
            }

            const auto outputPath = Detail::makePatchedOutputPath(path,
                                                                  state.loadOffset,
                                                                  desiredBuildTarget,
                                                                  fileSize,
                                                                  lastWrite.time_since_epoch().count());
            state.loadPath = outputPath.string();
            state.usedPatchedCopy = true;

            if (std::filesystem::exists(outputPath, ec) && !ec) {
                return true;
            }

            if (!Detail::writeFileBytesAtomically(outputPath, patchedBytes, state.error)) {
                state.usedPatchedCopy = false;
                state.loadPath = sourcePath;
                state.loadOffset = requestedOffset;
                return false;
            }
            return true;
        };

        if (tryPatchAtOffset(requestedOffset, result)) {
            return result;
        }

        if (result.error.empty()) {
            result.error = "failed to prepare patched load target";
        }
        return result;
    }
} // namespace LinkuraLocal::UnityAssetHelper
