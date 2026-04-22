#include "../HookMain.h"
#include "../UnityAssetHelper.hpp"
#include <chrono>
#include <thread>
#include <set>
#include <map>
#include <vector>
#include <future>
#include <atomic>
#include <mutex>

#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

namespace LinkuraLocal::HookDebug {
    using Il2cppString = UnityResolve::UnityType::String;

    static thread_local bool tls_insideAssetBundleLoadFromFile = false;

    // Unity AssetBundle build target enum (BuildTarget): Android=13(0x0D), iOS=9(0x09).
    // When loginAsIOS is enabled we still download iOS resources, but the Android Unity player expects
    // AssetBundle metadata to say Android. The reliable path is to rewrite the source file's
    // serialized m_TargetPlatform from iOS(9) to Android(13) before LoadFromFile_Internal reads it.
    namespace {
        constexpr uint16_t kBuildTargetAndroid = 0x0D;
        constexpr uint16_t kBuildTargetIos = 0x09;

        struct CmpImmPatchSite {
            uintptr_t addr = 0;      // instruction address
            uint32_t origInsn = 0;   // original 32-bit instruction word
        };

        static std::mutex g_buildTargetPatchMutex;
        static std::vector<CmpImmPatchSite> g_buildTargetPatchSites;
        static std::atomic<int> g_buildTargetPatchUsers{0};
        static uintptr_t g_unityLoadFromFileInternalAddr = 0; // resolved icall pointer

	        struct NopPatchSite {
	            uintptr_t addr = 0;
	            uint32_t origInsn = 0;
	            uint32_t patchedInsn = 0;
	        };
		        static std::vector<NopPatchSite> g_buildTargetBypassBranchSites;
		        static std::atomic<int> g_buildTargetBypassUsers{0};
		        static std::chrono::steady_clock::time_point g_buildTargetBypassLastScan{};
		        static int g_buildTargetBypassScanAttempts = 0;

        static bool is_cond_branch_insn(uint32_t insn) {
            // B.cond encoding: 01010100 (0x54) at [31:24], bit[4]==0, cond in [3:0]
            return (insn & 0xFF000010u) == 0x54000000u;
        }

        static uintptr_t sign_extend(uintptr_t x, int bits) {
            const uintptr_t m = static_cast<uintptr_t>(1) << (bits - 1);
            return (x ^ m) - m;
        }

	        static bool is_adrp(uint32_t insn) {
	            // ADRP: op=1, fixed bits [28:24]=10000, mask from ARM64 encoding patterns.
	            return (insn & 0x9F000000u) == 0x90000000u;
	        }

	        static bool is_adr(uint32_t insn) {
	            // ADR: op=0, fixed bits [28:24]=10000
	            return (insn & 0x9F000000u) == 0x10000000u;
	        }

	        static uintptr_t decode_adr_target(uint32_t insn, uintptr_t pc) {
	            // imm = sign_extend(immhi:immlo, 21); target = pc + imm
	            const uintptr_t immlo = (insn >> 29) & 0x3;
	            const uintptr_t immhi = (insn >> 5) & 0x7FFFF;
	            uintptr_t imm21 = (immhi << 2) | immlo;
	            imm21 = sign_extend(imm21, 21);
	            return pc + imm21;
	        }

	        static bool is_add_imm64(uint32_t insn) {
	            // ADD (immediate, 64-bit): 0x91000000 with varying fields.
	            return (insn & 0xFF000000u) == 0x91000000u;
	        }

        static uintptr_t decode_adrp_target(uint32_t insn, uintptr_t pc) {
            // imm = sign_extend(immhi:immlo, 21) << 12; target = (pc & ~0xFFF) + imm
            const uintptr_t immlo = (insn >> 29) & 0x3;
            const uintptr_t immhi = (insn >> 5) & 0x7FFFF;
            uintptr_t imm21 = (immhi << 2) | immlo;
            imm21 = sign_extend(imm21, 21);
            const uintptr_t imm = imm21 << 12;
            return (pc & ~static_cast<uintptr_t>(0xFFF)) + imm;
        }

	        static bool decode_add_imm64(uint32_t insn, uint32_t& rd, uint32_t& rn, uint32_t& imm12, uint32_t& shift) {
	            if (!is_add_imm64(insn)) return false;
	            rd = insn & 0x1F;
	            rn = (insn >> 5) & 0x1F;
	            imm12 = (insn >> 10) & 0xFFF;
	            shift = (insn >> 22) & 0x3;
	            return true;
	        }

		        static bool is_ldr_x_uimm(uint32_t insn) {
		            // LDR Xt, [Xn, #imm12*8]
		            return (insn & 0xFFC00000u) == 0xF9400000u;
		        }

		        static bool decode_ldr_x_uimm(uint32_t insn, uint32_t& rt, uint32_t& rn, uint32_t& imm12) {
		            if (!is_ldr_x_uimm(insn)) return false;
		            rt = insn & 0x1F;
		            rn = (insn >> 5) & 0x1F;
		            imm12 = (insn >> 10) & 0xFFF;
		            return true;
		        }

		        static bool is_ldr_lit64(uint32_t insn) {
		            // LDR Xt, #imm19 (literal, 64-bit)
		            return (insn & 0xFF000000u) == 0x58000000u;
		        }

		        static uintptr_t decode_ldr_lit_target(uint32_t insn, uintptr_t pc) {
		            // imm19 in bits [23:5], signed, <<2. Address is PC-relative.
		            uintptr_t imm19 = (insn >> 5) & 0x7FFFF;
		            imm19 = sign_extend(imm19, 19);
		            return pc + (imm19 << 2);
		        }

	        static bool is_cmp_imm_32(uint32_t insn, uint16_t imm12) {
	            // cmp wN, #imm == subs wzr, wN, #imm (shift=0)
	            // SUBS (immediate, 32-bit) base: 0x71000000, Rd=WZR => low5=0x1F
	            if ((insn & 0x7F00001Fu) != 0x7100001Fu) return false;
            if (((insn >> 22) & 0x3u) != 0) return false; // shift must be 0
            return ((insn >> 10) & 0xFFFu) == imm12;
        }

        static bool is_cmp_imm_64(uint32_t insn, uint16_t imm12) {
            // cmp xN, #imm == subs xzr, xN, #imm (shift=0)
            if ((insn & 0xFF00001Fu) != 0xF100001Fu) return false;
            if (((insn >> 22) & 0x3u) != 0) return false; // shift must be 0
            return ((insn >> 10) & 0xFFFu) == imm12;
        }

        static bool write_code_u32(uintptr_t addr, uint32_t value) {
            const long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize <= 0) return false;
            const uintptr_t pageStart = addr & ~(static_cast<uintptr_t>(pageSize) - 1);
            if (mprotect(reinterpret_cast<void*>(pageStart), static_cast<size_t>(pageSize),
                         PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
                return false;
            }
            *reinterpret_cast<volatile uint32_t*>(addr) = value;
            __builtin___clear_cache(reinterpret_cast<char*>(pageStart),
                                    reinterpret_cast<char*>(pageStart + pageSize));
            // Best-effort restore to RX.
            (void)mprotect(reinterpret_cast<void*>(pageStart), static_cast<size_t>(pageSize),
                           PROT_READ | PROT_EXEC);
            return true;
        }

        static uintptr_t find_mapping_start(uintptr_t addr) {
            FILE* fp = std::fopen("/proc/self/maps", "r");
            if (!fp) return 0;
            char line[512];
            uintptr_t start = 0, end = 0;
            while (std::fgets(line, sizeof(line), fp)) {
                start = 0; end = 0;
                if (std::sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    if (addr >= start && addr < end) {
                        std::fclose(fp);
                        return start;
                    }
                }
            }
            std::fclose(fp);
            return 0;
        }

        static uintptr_t find_mapping_end(uintptr_t addr) {
            // Parse /proc/self/maps to find the mapping containing addr.
            // Format: start-end perms offset dev inode pathname?
            FILE* fp = std::fopen("/proc/self/maps", "r");
            if (!fp) return 0;
            char line[512];
            uintptr_t start = 0, end = 0;
            while (std::fgets(line, sizeof(line), fp)) {
                start = 0; end = 0;
                if (std::sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    if (addr >= start && addr < end) {
                        std::fclose(fp);
                        return end;
                    }
                }
            }
            std::fclose(fp);
            return 0;
        }

	        struct MapsSeg {
	            uintptr_t start = 0;
	            uintptr_t end = 0;
	            char perms[5] = {0};
	            unsigned long inode = 0;
	            char path[256] = {0};
	        };

	        static std::vector<MapsSeg> list_all_segments() {
	            std::vector<MapsSeg> segs;
	            FILE* fp = std::fopen("/proc/self/maps", "r");
	            if (!fp) return segs;
	            char line[1024];
	            while (std::fgets(line, sizeof(line), fp)) {
	                MapsSeg s;
	                unsigned long st = 0, ed = 0;
	                char perms[5] = {0};
	                unsigned long inode = 0;
	                char path[256] = {0};
	                // pathname is optional
	                // start-end perms offset dev inode pathname?
	                // Conversions: st, ed, perms, inode, path  => max n = 5
	                const int n = std::sscanf(line, "%lx-%lx %4s %*s %*s %lu %255s", &st, &ed, perms, &inode, path);
	                if (n < 3) continue;
	                s.start = static_cast<uintptr_t>(st);
	                s.end = static_cast<uintptr_t>(ed);
	                s.inode = (n >= 4) ? inode : 0;
	                std::snprintf(s.perms, sizeof(s.perms), "%s", perms);
	                if (n >= 5) {
	                    std::snprintf(s.path, sizeof(s.path), "%s", path);
	                } else {
	                    s.path[0] = '\0';
	                }
	                segs.push_back(s);
	            }
	            std::fclose(fp);
	            return segs;
	        }

	        static bool find_segment_for_addr(const std::vector<MapsSeg>& segs, uintptr_t addr, MapsSeg& out) {
	            for (const auto& s : segs) {
	                if (addr >= s.start && addr < s.end) {
	                    out = s;
	                    return true;
	                }
	            }
	            return false;
	        }

	        static std::vector<MapsSeg> filter_segments_by_inode(const std::vector<MapsSeg>& segs, unsigned long inode) {
	            std::vector<MapsSeg> out;
	            if (!inode) return out;
	            out.reserve(16);
	            for (const auto& s : segs) {
	                if (s.inode == inode) out.push_back(s);
	            }
	            return out;
	        }

	        static std::vector<MapsSeg> filter_segments_by_path_substr(const std::vector<MapsSeg>& segs, const char* substr) {
	            std::vector<MapsSeg> out;
	            if (!substr || !*substr) return out;
	            out.reserve(16);
	            for (const auto& s : segs) {
	                if (std::strstr(s.path, substr)) out.push_back(s);
	            }
	            return out;
	        }

	        static const uint8_t* memmem_u8(const uint8_t* hay, size_t hayLen, const char* needle) {
	            if (!hay || !needle) return nullptr;
	            const size_t nLen = std::strlen(needle);
	            if (nLen == 0 || hayLen < nLen) return nullptr;
	            for (size_t i = 0; i + nLen <= hayLen; i++) {
	                if (std::memcmp(hay + i, needle, nLen) == 0) return hay + i;
	            }
	            return nullptr;
	        }

	        static uintptr_t find_cstr_start(const uint8_t* segStart, const uint8_t* p, size_t maxBack = 0x200) {
	            // `p` points inside a C-string. Walk backwards to find its start (after previous '\0').
	            if (!segStart || !p) return reinterpret_cast<uintptr_t>(p);
	            const uint8_t* cur = p;
	            size_t walked = 0;
	            while (cur > segStart && walked < maxBack) {
	                if (*(cur - 1) == 0) break;
	                cur--;
	                walked++;
	            }
	            return reinterpret_cast<uintptr_t>(cur);
	        }

	        static bool is_readable_range(uintptr_t addr, size_t size) {
	            if (size == 0) return true;
	            const uintptr_t end = addr + size;
	            FILE* fp = std::fopen("/proc/self/maps", "r");
	            if (!fp) return false;
	            char line[512];
	            uintptr_t start = 0, stop = 0;
	            char perms[5] = {0};
	            while (std::fgets(line, sizeof(line), fp)) {
	                start = 0; stop = 0; perms[0] = 0;
	                if (std::sscanf(line, "%lx-%lx %4s", &start, &stop, perms) == 3) {
	                    if (addr >= start && end <= stop) {
	                        std::fclose(fp);
	                        return perms[0] == 'r';
	                    }
	                }
	            }
	            std::fclose(fp);
	            return false;
	        }

	        static bool is_cbz_cbnz(uint32_t insn) {
	            // CBZ/CBNZ: opcodes 0x34/0x35 in top bits. Mask: 0x7F000000 == 0x34000000.
	            return (insn & 0x7F000000u) == 0x34000000u;
	        }

        static bool is_tbz_tbnz(uint32_t insn) {
            // TBZ/TBNZ: 0x36000000 with mask 0x7F000000 == 0x36000000.
            return (insn & 0x7F000000u) == 0x36000000u;
        }

        static uintptr_t decode_bcond_target(uint32_t insn, uintptr_t pc) {
            // imm19 in bits [23:5], signed, <<2.
            uintptr_t imm19 = (insn >> 5) & 0x7FFFF;
            imm19 = sign_extend(imm19, 19);
            return pc + (imm19 << 2);
        }

        static uintptr_t decode_cb_target(uint32_t insn, uintptr_t pc) {
            // imm19 in bits [23:5], signed, <<2.
            uintptr_t imm19 = (insn >> 5) & 0x7FFFF;
            imm19 = sign_extend(imm19, 19);
            return pc + (imm19 << 2);
        }

	        static uintptr_t decode_tb_target(uint32_t insn, uintptr_t pc) {
	            // imm14 in bits [18:5], signed, <<2.
	            uintptr_t imm14 = (insn >> 5) & 0x3FFF;
	            imm14 = sign_extend(imm14, 14);
	            return pc + (imm14 << 2);
	        }

	        static bool encode_uncond_b(uintptr_t pc, uintptr_t target, uint32_t& outInsn) {
	            // Unconditional B: 0x14000000 | imm26, where imm26 = (target - pc) >> 2 (signed).
	            const intptr_t delta = static_cast<intptr_t>(target) - static_cast<intptr_t>(pc);
	            if ((delta & 0x3) != 0) return false;
	            const intptr_t imm26 = delta >> 2;
	            if (imm26 < -(1 << 25) || imm26 >= (1 << 25)) return false; // signed 26-bit
	            outInsn = 0x14000000u | (static_cast<uint32_t>(imm26) & 0x03FFFFFFu);
	            return true;
	        }

		        static void ensure_build_target_bypass_sites_locked() {
		            if (!g_buildTargetBypassBranchSites.empty()) return;

		            const auto allSegs = list_all_segments();
		            if (allSegs.empty()) return;

		            // Retry scanning periodically until we find patch sites. Unity may map modules/strings lazily.
		            const auto now = std::chrono::steady_clock::now();
		            if (g_buildTargetBypassScanAttempts > 0) {
		                const auto dt = now - g_buildTargetBypassLastScan;
		                if (dt < std::chrono::seconds(3)) return;
		            }
		            g_buildTargetBypassLastScan = now;
		            g_buildTargetBypassScanAttempts++;
		            Log::WarnFmt("BuildTarget bypass: scan attempt %d (icall=%p)", g_buildTargetBypassScanAttempts,
		                         (void*)g_unityLoadFromFileInternalAddr);

		            // Prefer scanning within the same mapped module that contains the resolved icall pointer.
		            std::vector<MapsSeg> moduleSegs;
		            MapsSeg icallSeg{};
		            if (g_unityLoadFromFileInternalAddr && find_segment_for_addr(allSegs, g_unityLoadFromFileInternalAddr, icallSeg)) {
	                if (icallSeg.inode) {
	                    moduleSegs = filter_segments_by_inode(allSegs, icallSeg.inode);
	                } else if (icallSeg.path[0]) {
	                    moduleSegs = filter_segments_by_path_substr(allSegs, icallSeg.path);
	                }
	            }
		            if (moduleSegs.empty()) {
		                // Fallback: try classic name; if even that fails, scan globally for the needle.
		                moduleSegs = filter_segments_by_path_substr(allSegs, "libunity.so");
		            }

		            if (moduleSegs.empty()) {
		                Log::WarnFmt("BuildTarget bypass: module identification failed (icall=%p segPath=%s inode=%lu).",
		                             (void*)g_unityLoadFromFileInternalAddr, icallSeg.path, icallSeg.inode);
		                return;
		            }
		            Log::WarnFmt("BuildTarget bypass: icall seg %p-%p perms=%s inode=%lu path=%s",
		                         (void*)icallSeg.start, (void*)icallSeg.end, icallSeg.perms, icallSeg.inode, icallSeg.path);
		            Log::WarnFmt("BuildTarget bypass: scanning module segments=%zu", moduleSegs.size());

		            constexpr const char* kNeedle = "File's Build target is:";
		            uintptr_t hitAddr = 0;
		            uintptr_t strAddr = 0; // actual C-string start
		            MapsSeg needleSeg{};
	            for (const auto& s : moduleSegs) {
	                if (s.perms[0] != 'r') continue;
	                const auto* mem = reinterpret_cast<const uint8_t*>(s.start);
	                const size_t len = static_cast<size_t>(s.end - s.start);
	                if (const uint8_t* hit = memmem_u8(mem, len, kNeedle)) {
	                    hitAddr = reinterpret_cast<uintptr_t>(hit);
	                    strAddr = find_cstr_start(mem, hit);
	                    needleSeg = s;
	                    break;
	                }
	            }
	            if (!strAddr) {
	                // As a fallback, scan all segments globally for the needle (string may live in a different module).
	                for (const auto& s : allSegs) {
	                    if (s.perms[0] != 'r') continue;
	                    const auto* mem = reinterpret_cast<const uint8_t*>(s.start);
	                    const size_t len = static_cast<size_t>(s.end - s.start);
	                    if (const uint8_t* hit = memmem_u8(mem, len, kNeedle)) {
	                        hitAddr = reinterpret_cast<uintptr_t>(hit);
	                        strAddr = find_cstr_start(mem, hit);
	                        needleSeg = s;
	                        break;
	                    }
	                }
	            }
		            if (!strAddr) {
		                Log::WarnFmt("BuildTarget bypass: string needle not found.");
		                return;
		            }
		            Log::WarnFmt("BuildTarget bypass: needle hit=%p cstr=%p seg %p-%p perms=%s inode=%lu path=%s",
		                         (void*)hitAddr, (void*)strAddr,
		                         (void*)needleSeg.start, (void*)needleSeg.end, needleSeg.perms, needleSeg.inode, needleSeg.path);
	            {
	                char preview[96] = {0};
	                std::strncpy(preview, reinterpret_cast<const char*>(strAddr), sizeof(preview) - 1);
	                Log::WarnFmt("BuildTarget bypass: preview=\"%s\"", preview);
	            }

	            // If the string is in a different module, re-scope scans to that module.
		            if (needleSeg.inode && (!icallSeg.inode || needleSeg.inode != icallSeg.inode)) {
		                moduleSegs = filter_segments_by_inode(allSegs, needleSeg.inode);
		                Log::WarnFmt("BuildTarget bypass: rescoping to needle inode=%lu segments=%zu", needleSeg.inode, moduleSegs.size());
		            }

		            // Find a code site that materializes the string address (in the same module as the string).
		            uintptr_t codeRefPc = 0;
		            const char* xrefKind = nullptr;
		            for (const auto& s : moduleSegs) {
		                if (s.perms[0] != 'r') continue;
		                if (std::strchr(s.perms, 'x') == nullptr) continue;
		                const uintptr_t start = s.start;
		                const uintptr_t end = s.end;
		                for (uintptr_t pc = start; pc + 8 <= end; pc += 4) {
		                    const uint32_t i0 = *reinterpret_cast<const uint32_t*>(pc);
			                    if (is_adr(i0)) {
			                        const uintptr_t full = decode_adr_target(i0, pc);
			                        if (full == strAddr || (hitAddr && full == hitAddr)) {
			                            codeRefPc = pc;
			                            xrefKind = "ADR";
			                            break;
			                        }
			                        continue;
			                    }
			                    if (is_ldr_lit64(i0)) {
			                        const uintptr_t lit = decode_ldr_lit_target(i0, pc);
			                        if (is_readable_range(lit, sizeof(uintptr_t))) {
			                            const uintptr_t ptr = *reinterpret_cast<const uintptr_t*>(lit);
			                            if (ptr == strAddr || (hitAddr && ptr == hitAddr)) {
			                                codeRefPc = pc;
			                                xrefKind = "LDR-literal";
			                                Log::WarnFmt("BuildTarget bypass: xref via LDR literal at %p (lit=%p ptr=%p)",
			                                             (void*)pc, (void*)lit, (void*)ptr);
			                                break;
			                            }
			                        }
			                    }

		                    if (!is_adrp(i0)) continue;
		                    const uint32_t baseReg = i0 & 0x1F;
		                    const uintptr_t page = decode_adrp_target(i0, pc);

		                    // Unity often does ADRP, then a few instructions, then ADD/LDR to materialize the pointer.
		                    constexpr int kLookaheadInsns = 8;
		                    for (int k = 1; k <= kLookaheadInsns && pc + 4u * k < end; k++) {
		                        const uint32_t insn = *reinterpret_cast<const uint32_t*>(pc + 4u * k);

		                        uint32_t rd1 = 0, rn1 = 0, imm12 = 0, shift = 0;
		                        if (decode_add_imm64(insn, rd1, rn1, imm12, shift)) {
		                            if (rn1 == baseReg && (shift == 0 || shift == 1)) {
		                                const uintptr_t full = page + (shift == 1 ? (static_cast<uintptr_t>(imm12) << 12) : static_cast<uintptr_t>(imm12));
		                                if (full == strAddr || (hitAddr && full == hitAddr)) {
		                                    codeRefPc = pc;
		                                    xrefKind = "ADRP..ADD";
		                                    Log::WarnFmt("BuildTarget bypass: xref via ADRP..ADD at %p (+%d)", (void*)pc, k);
		                                    break;
		                                }
		                            }
		                        }

		                        uint32_t rt = 0, rn = 0, ldrImm12 = 0;
		                        if (decode_ldr_x_uimm(insn, rt, rn, ldrImm12)) {
		                            if (rn == baseReg) {
		                                const uintptr_t slot = page + (static_cast<uintptr_t>(ldrImm12) << 3);
		                                if (!is_readable_range(slot, sizeof(uintptr_t))) continue;
		                                const uintptr_t ptr = *reinterpret_cast<const uintptr_t*>(slot);
		                                if (ptr == strAddr || (hitAddr && ptr == hitAddr)) {
		                                    codeRefPc = pc;
		                                    xrefKind = "ADRP..LDR";
		                                    Log::WarnFmt("BuildTarget bypass: xref via ADRP..LDR at %p (+%d) (slot=%p ptr=%p rt=%u)",
		                                                 (void*)pc, k, (void*)slot, (void*)ptr, rt);
		                                    break;
		                                }
		                            }
		                        }
		                    }
		                    if (codeRefPc) break;
		                }
		                if (codeRefPc) break;
		            }

		            if (!codeRefPc) {
		                Log::WarnFmt("BuildTarget bypass: no code xref found (cstr=%p hit=%p).", (void*)strAddr, (void*)hitAddr);
		                return;
		            }
		            Log::WarnFmt("BuildTarget bypass: picked xref=%p kind=%s", (void*)codeRefPc, xrefKind ? xrefKind : "?");

	            // Find a nearby conditional branch controlling the error block containing codeRefPc.
	            // Two possible layouts:
	            // - branch enters error: target near codeRefPc -> NOP branch
	            // - branch skips error: target just after codeRefPc -> force always-taken by rewriting to unconditional B
	            constexpr intptr_t kBackScan = 0x300;
	            constexpr intptr_t kEnterNearLo = -0x80;
	            constexpr intptr_t kEnterNearHi = 0x120;
	            constexpr intptr_t kSkipMin = 0x10;
	            constexpr intptr_t kSkipMax = 0x240;

	            const uintptr_t scanStart = codeRefPc > static_cast<uintptr_t>(kBackScan) ? (codeRefPc - kBackScan) : codeRefPc;
	            uintptr_t bestPc = 0;
	            uint32_t bestOrig = 0;
	            uint32_t bestPatched = 0;
	            const char* bestMode = nullptr;
	            for (uintptr_t pc = scanStart; pc < codeRefPc; pc += 4) {
	                const uint32_t insn = *reinterpret_cast<const uint32_t*>(pc);
	                uintptr_t target = 0;
	                if (is_cond_branch_insn(insn)) {
	                    target = decode_bcond_target(insn, pc);
	                } else if (is_cbz_cbnz(insn)) {
	                    target = decode_cb_target(insn, pc);
	                } else if (is_tbz_tbnz(insn)) {
	                    target = decode_tb_target(insn, pc);
	                } else {
	                    continue;
	                }

	                // Enter-error branch (target into error block)
	                const intptr_t enterDelta = static_cast<intptr_t>(target) - static_cast<intptr_t>(codeRefPc);
	                if (enterDelta >= kEnterNearLo && enterDelta <= kEnterNearHi) {
	                    if (pc >= bestPc) {
	                        bestPc = pc;
	                        bestOrig = insn;
	                        bestPatched = 0xD503201F; // NOP
	                        bestMode = "NopBranch";
	                    }
	                    continue;
	                }

	                // Skip-error branch (target after error block)
	                const intptr_t skipDelta = static_cast<intptr_t>(target) - static_cast<intptr_t>(codeRefPc);
	                if (skipDelta >= kSkipMin && skipDelta <= kSkipMax) {
	                    uint32_t bInsn = 0;
	                    if (encode_uncond_b(pc, target, bInsn)) {
	                        if (pc >= bestPc) {
	                            bestPc = pc;
	                            bestOrig = insn;
	                            bestPatched = bInsn;
	                            bestMode = "ForceBranch";
	                        }
	                    }
	                }
	            }

	            if (!bestPc) {
	                Log::WarnFmt("BuildTarget bypass: no nearby conditional branch found for error block near %p.", (void*)codeRefPc);
	                return;
	            }

		            g_buildTargetBypassBranchSites.push_back({bestPc, bestOrig, bestPatched});
		            Log::WarnFmt("BuildTarget bypass: will patch 1 branch at %p (mode=%s, xref=%p, needle=%p).",
		                         (void*)bestPc, bestMode ? bestMode : "?", (void*)codeRefPc, (void*)strAddr);
		        }

        static void apply_build_target_patch_locked(uint16_t desiredImm12) {
            for (auto& site : g_buildTargetPatchSites) {
                // Rewrite only imm12 bits (21:10) based on the original instruction word.
                const uint32_t patched = (site.origInsn & ~(0xFFFu << 10)) | (static_cast<uint32_t>(desiredImm12) << 10);
                (void)write_code_u32(site.addr, patched);
            }
        }

	        static void apply_build_target_bypass_locked(bool enable) {
	            for (auto& s : g_buildTargetBypassBranchSites) {
	                (void)write_code_u32(s.addr, enable ? s.patchedInsn : s.origInsn);
	            }
	        }

        static void ensure_build_target_patch_sites_locked() {
            if (g_unityLoadFromFileInternalAddr == 0) return;
            if (!g_buildTargetPatchSites.empty()) return;

            // Scan a small window in the native function for "cmp #0x0D; b.cond" patterns.
            // We patch those compares to iOS (0x09) while inside iOS AssetBundle loads.
            constexpr size_t kMaxScanBytes = 0x3000;
            constexpr size_t kMinOffset = 0x40; // skip prologue/trampoline region

            const auto base = g_unityLoadFromFileInternalAddr;
            size_t scanBytes = kMaxScanBytes;
            if (const uintptr_t mappingEnd = find_mapping_end(base)) {
                const uintptr_t avail = mappingEnd - base;
                if (avail < scanBytes) scanBytes = static_cast<size_t>(avail);
            }
            if (scanBytes <= kMinOffset + 8) {
                Log::WarnFmt("BuildTarget patch: mapping too small to scan (%p).", (void*)base);
                return;
            }

            size_t found = 0;
            for (size_t off = kMinOffset; off + 8 <= scanBytes; off += 4) {
                const uint32_t insn = *reinterpret_cast<const uint32_t*>(base + off);
                const uint32_t next = *reinterpret_cast<const uint32_t*>(base + off + 4);
                if (!is_cond_branch_insn(next)) continue;

                if (is_cmp_imm_32(insn, kBuildTargetAndroid) || is_cmp_imm_64(insn, kBuildTargetAndroid)) {
                    g_buildTargetPatchSites.push_back({base + off, insn});
                    found++;
                    // Avoid patching too many sites; if there are lots, our heuristic is probably wrong.
                    if (found >= 8) break;
                }
            }

            if (g_buildTargetPatchSites.empty()) {
                Log::WarnFmt("BuildTarget patch: no cmp-immediate sites found near LoadFromFile_Internal (%p).", (void*)base);
            } else {
                Log::InfoFmt("BuildTarget patch: found %zu candidate cmp sites near LoadFromFile_Internal (%p).", g_buildTargetPatchSites.size(), (void*)base);
            }
        }

        struct ScopedBuildTargetPatch {
            bool active = false;
            ScopedBuildTargetPatch() = default;
            explicit ScopedBuildTargetPatch(bool enable) {
                if (!enable) return;
                std::lock_guard<std::mutex> lock(g_buildTargetPatchMutex);
                ensure_build_target_patch_sites_locked();
                if (g_buildTargetPatchSites.empty()) return;

                const int prev = g_buildTargetPatchUsers.fetch_add(1, std::memory_order_acq_rel);
                if (prev == 0) {
                    apply_build_target_patch_locked(kBuildTargetIos);
                }
                active = true;
            }
            ~ScopedBuildTargetPatch() {
                if (!active) return;
                std::lock_guard<std::mutex> lock(g_buildTargetPatchMutex);
                const int now = g_buildTargetPatchUsers.fetch_sub(1, std::memory_order_acq_rel) - 1;
                if (now == 0 && !g_buildTargetPatchSites.empty()) {
                    apply_build_target_patch_locked(kBuildTargetAndroid);
                }
            }
        };

        struct ScopedBuildTargetBypass {
            bool active = false;
            ScopedBuildTargetBypass() = default;
            explicit ScopedBuildTargetBypass(bool enable) {
                if (!enable) return;
                std::lock_guard<std::mutex> lock(g_buildTargetPatchMutex);
                ensure_build_target_bypass_sites_locked();
                if (g_buildTargetBypassBranchSites.empty()) return;

                const int prev = g_buildTargetBypassUsers.fetch_add(1, std::memory_order_acq_rel);
                if (prev == 0) {
                    apply_build_target_bypass_locked(true);
                }
                active = true;
            }
            ~ScopedBuildTargetBypass() {
                if (!active) return;
                std::lock_guard<std::mutex> lock(g_buildTargetPatchMutex);
                const int now = g_buildTargetBypassUsers.fetch_sub(1, std::memory_order_acq_rel) - 1;
                if (now == 0 && !g_buildTargetBypassBranchSites.empty()) {
                    apply_build_target_bypass_locked(false);
                }
            }
        };
    } // namespace

	    static std::string replaceAndroidDirWithIos(std::string s) {
	        // Only swap the platform path component. This is intentionally narrow to avoid accidental replacements.
	        constexpr std::string_view fromMid = "/android";
	        constexpr std::string_view toMid = "/ios";

        size_t pos = 0;
        while ((pos = s.find(fromMid, pos)) != std::string::npos) {
            s.replace(pos, fromMid.size(), toMid);
            pos += toMid.size();
        }
        // Handle trailing "/android" (no trailing slash)
        if (s.size() >= 8 && s.rfind("/android") == s.size() - 8) {
            s.replace(s.size() - 8, 8, "/ios");
        }
	        return s;
	    }

	    DEFINE_HOOK(void, Internal_LogException, (void* ex, void* obj)) {
	        Internal_LogException_Orig(ex, obj);
	        static auto Exception_ToString = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Exception", "ToString");
	        Log::LogUnityLog(ANDROID_LOG_ERROR, "UnityLog - Internal_LogException:\n%s", Exception_ToString->Invoke<Il2cppString*>(ex)->ToString().c_str());
	    }

    DEFINE_HOOK(void, Internal_Log, (int logType, int logOption, UnityResolve::UnityType::String* content, void* context)) {
        Internal_Log_Orig(logType, logOption, content, context);
        // 2022.3.21f1
        Log::LogUnityLog(ANDROID_LOG_VERBOSE, "Internal_Log:\n%s", content->ToString().c_str());
    }

    // 👀
    DEFINE_HOOK(void, CoverImageCommandReceiver_Awake, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("CoverImageCommandReceiver_Awake HOOKED");
        if (Config::removeRenderImageCover) return;
        CoverImageCommandReceiver_Awake_Orig(self, method);
    }
    // 👀　work for both als and mrs
    DEFINE_HOOK(void, CharacterVisibleReceiver_SetupExistCharacter, (Il2cppUtils::Il2CppObject* self,int character, void* method)) {
        Log::DebugFmt("CharacterVisibleReceiver_SetupExistCharacter HOOKED");
        if (Config::avoidCharacterExit) return;
        CharacterVisibleReceiver_SetupExistCharacter_Orig(self, character, method);
    }
    // old Config::enableLegacyCompatibility
    DEFINE_HOOK(void, CharacterVisibleReceiver_UpdateAvatarVisibility, (Il2cppUtils::Il2CppObject* self, bool isVisible, void* method)) {
        Log::DebugFmt("CharacterVisibleReceiver_UpdateAvatarVisibility HOOKED");
        if (Config::avoidCharacterExit) isVisible = true;
        CharacterVisibleReceiver_UpdateAvatarVisibility_Orig(self, isVisible, method);
    }
    // old Config::enableLegacyCompatibility
    DEFINE_HOOK(void, MRS_AppsCoverScreen_SetActiveCoverImage, (Il2cppUtils::Il2CppObject* self, bool isActive, void* method)) {
        Log::DebugFmt("AppsCoverScreen_SetActiveCoverImage HOOKED");
        if (Config::removeRenderImageCover) isActive = false;
        MRS_AppsCoverScreen_SetActiveCoverImage_Orig(self, isActive, method);
    }

    DEFINE_HOOK(Il2cppString*, Hailstorm_AssetDownloadJob_get_UrlBase, (Il2cppUtils::Il2CppObject* self, void* method)) {
        auto base = Hailstorm_AssetDownloadJob_get_UrlBase_Orig(self, method);
        if (base) {
            auto s = base->ToString();
            if (!Config::assetsUrlPrefix.empty()) {
                s = HookShare::replaceUriHost(s, Config::assetsUrlPrefix);
            }
            if (Config::loginAsIOS) {
                // Force platform dir to ios so we request "/ios/..." instead of "/android/...".
                s = replaceAndroidDirWithIos(std::move(s));
            }
            base = Il2cppString::New(s);
        }
        return base;
    }

    DEFINE_HOOK(Il2cppString*, Hailstorm_AssetDownloadJob_get_LocalBase, (Il2cppUtils::Il2CppObject* self, void* method)) {
        auto base = Hailstorm_AssetDownloadJob_get_LocalBase_Orig(self, method);
        if (base && Config::loginAsIOS) {
            // Force local cache dir to ios so files land under ".../ios/..."
            base = Il2cppString::New(replaceAndroidDirWithIos(base->ToString()));
        }
        return base;
    }

    DEFINE_HOOK(int, UnityEngine_Application_get_platform, ()) {
        return UnityEngine_Application_get_platform_Orig();
    }

    DEFINE_HOOK(void*, UnityEngine_AssetBundle_LoadFromFile_Internal, (Il2cppString* path, uint32_t crc, uint64_t offset)) {
        if (Config::loginAsIOS) {
            static bool s_loggedOnce = false;
            static std::mutex s_pathLogMutex;
            static std::set<std::string> s_loggedPaths;
            static std::mutex s_patchLogMutex;
            static std::set<std::string> s_patchLoggedPaths;
            if (!s_loggedOnce) {
                s_loggedOnce = true;
                Log::WarnFmt("BuildTarget bypass: hook entered (icall=%p).", (void*)g_unityLoadFromFileInternalAddr);
            }
            if (path) {
                const auto p0 = path->ToString();
                if (!p0.empty()) {
                    bool doLog = false;
                    {
                        std::lock_guard<std::mutex> lock(s_pathLogMutex);
                        if (s_loggedPaths.insert(p0).second) doLog = true;
                    }
                    if (doLog) {
                        Log::WarnFmt("AssetBundle.LoadFromFile_Internal: path=%s crc=%u offset=%llu",
                                     p0.c_str(), crc, static_cast<unsigned long long>(offset));
                    }
                }
            }

            tls_insideAssetBundleLoadFromFile = true;
            // The primary strategy is now to rewrite the on-disk source bundle target to Android(13)
            // before Unity reads it. Keep the native bypass disabled unless we prove it is needed again.
            const ScopedBuildTargetBypass bypassGuard(false);
            const ScopedBuildTargetPatch patchGuard(false);

            if (path) {
                const auto originalPath = path->ToString();
                if (!originalPath.empty()) {
                    const auto patchResult = UnityAssetHelper::PatchFileTargetBuildIdInPlace(originalPath, offset, kBuildTargetAndroid);
                    bool shouldLogPatchResult = false;
                    {
                        std::lock_guard<std::mutex> lock(s_patchLogMutex);
                        shouldLogPatchResult = s_patchLoggedPaths.insert(originalPath).second;
                    }
                    if (patchResult.patched) {
                        crc = 0;
                        if (shouldLogPatchResult) {
                            const auto directFileOffsetStr = patchResult.firstDirectFileOffsetValid
                                                             ? Log::StringFormat("0x%llX", static_cast<unsigned long long>(patchResult.firstDirectFileOffset))
                                                             : std::string("n/a");
                            Log::WarnFmt("AssetBundle target patch: path=%s target=0x%02X patched=%zu observed=0x%02X requestOffset=%llu actualOffset=%llu nodeOffset=0x%llX nodeSize=0x%llX targetOffsetInNode=0x%llX targetRawOffset=0x%llX blockIndex=%lld blockCompression=%u directFileOffset=%s",
                                         originalPath.c_str(),
                                         kBuildTargetAndroid,
                                         patchResult.patchedSerializedFileCount,
                                         patchResult.firstObservedBuildTarget,
                                         static_cast<unsigned long long>(offset),
                                         static_cast<unsigned long long>(patchResult.actualOffset),
                                         static_cast<unsigned long long>(patchResult.firstNodeOffset),
                                         static_cast<unsigned long long>(patchResult.firstNodeSize),
                                         static_cast<unsigned long long>(patchResult.firstTargetOffsetInNode),
                                         static_cast<unsigned long long>(patchResult.firstTargetAbsoluteRawOffset),
                                         static_cast<long long>(patchResult.firstBlockIndex),
                                         patchResult.firstBlockCompression,
                                         directFileOffsetStr.c_str());
                            Log::WarnFmt("AssetBundle target patch: source file modified in place: %s", originalPath.c_str());
                        }
                    } else if (patchResult.targetAlreadyMatched) {
                        if (shouldLogPatchResult) {
                            const auto directFileOffsetStr = patchResult.firstDirectFileOffsetValid
                                                             ? Log::StringFormat("0x%llX", static_cast<unsigned long long>(patchResult.firstDirectFileOffset))
                                                             : std::string("n/a");
                            Log::WarnFmt("AssetBundle target patch: path=%s already target=0x%02X requestOffset=%llu actualOffset=%llu nodeOffset=0x%llX nodeSize=0x%llX targetOffsetInNode=0x%llX targetRawOffset=0x%llX blockIndex=%lld blockCompression=%u directFileOffset=%s",
                                         originalPath.c_str(),
                                         kBuildTargetAndroid,
                                         static_cast<unsigned long long>(offset),
                                         static_cast<unsigned long long>(patchResult.actualOffset),
                                         static_cast<unsigned long long>(patchResult.firstNodeOffset),
                                         static_cast<unsigned long long>(patchResult.firstNodeSize),
                                         static_cast<unsigned long long>(patchResult.firstTargetOffsetInNode),
                                         static_cast<unsigned long long>(patchResult.firstTargetAbsoluteRawOffset),
                                         static_cast<long long>(patchResult.firstBlockIndex),
                                         patchResult.firstBlockCompression,
                                         directFileOffsetStr.c_str());
                        }
                    } else if (!patchResult.error.empty() && shouldLogPatchResult) {
                        const auto directFileOffsetStr = patchResult.firstDirectFileOffsetValid
                                                         ? Log::StringFormat("0x%llX", static_cast<unsigned long long>(patchResult.firstDirectFileOffset))
                                                         : std::string("n/a");
                        Log::WarnFmt("AssetBundle target patch skipped: path=%s requestOffset=%llu actualOffset=%llu found=%s observed=0x%02X targetOffsetInNode=0x%llX targetRawOffset=0x%llX blockIndex=%lld blockCompression=%u directFileOffset=%s error=%s",
                                     originalPath.c_str(),
                                     static_cast<unsigned long long>(offset),
                                     static_cast<unsigned long long>(patchResult.actualOffset),
                                     patchResult.foundSerializedFile ? "true" : "false",
                                     patchResult.firstObservedBuildTarget,
                                     static_cast<unsigned long long>(patchResult.firstTargetOffsetInNode),
                                     static_cast<unsigned long long>(patchResult.firstTargetAbsoluteRawOffset),
                                     static_cast<long long>(patchResult.firstBlockIndex),
                                     patchResult.firstBlockCompression,
                                     directFileOffsetStr.c_str(),
                                     patchResult.error.c_str());
                    }
                }
            }

            auto ret = UnityEngine_AssetBundle_LoadFromFile_Internal_Orig(path, crc, offset);
            tls_insideAssetBundleLoadFromFile = false;
            if (path) {
                const auto finalPath = path->ToString();
                if (!ret) {
                    Log::WarnFmt("AssetBundle.LoadFromFile_Internal returned NULL (path=%s crc=%u offset=%llu).",
                                 finalPath.c_str(), crc, static_cast<unsigned long long>(offset));
                } else {
                    Log::WarnFmt("AssetBundle.LoadFromFile_Internal succeeded (path=%s ret=%p crc=%u offset=%llu).",
                                 finalPath.c_str(), ret, crc, static_cast<unsigned long long>(offset));
                }
            }
            return ret;
        }

        tls_insideAssetBundleLoadFromFile = false;
        return UnityEngine_AssetBundle_LoadFromFile_Internal_Orig(path, crc, offset);
    }

    DEFINE_HOOK(void, FootShadowManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FootShadowManipulator_OnInstantiate HOOKED");
        if (Config::hideCharacterShadow) return;
        FootShadowManipulator_OnInstantiate_Orig(self, method);
    }

    // character　item
    DEFINE_HOOK(void, ItemManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("ItemManipulator_OnInstantiate HOOKED");
        if (Config::hideLiveStreamCharacterItems) return;
        ItemManipulator_OnInstantiate_Orig(self, method);
    }

    enum HideLiveStreamSceneItemMode {
        None,
        /**
         * Simply hide live scene item, useful for with meets.\n
         */
        Lite,
        /**
         * Simply hide live scene item, useful for with meets.\n
         * Will remove static items for fes live.
         * And will try to hide static scene object
         */
        Normal,
        /**
         * Will try to remove dynamic items for fes live.
         */
        Strong,
        /**
         * Will remove timeline for fes live, light render, position control, dynamic camera will be useless.
         */
        Ultimate
    };

    // For example: Whiteboard, photo in with meets
    DEFINE_HOOK(void, ScenePropManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("ScenePropManipulator_OnInstantiate HOOKED");
        if (Config::hideLiveStreamSceneItemsLevel >= HideLiveStreamSceneItemMode::Lite) return;
        ScenePropManipulator_OnInstantiate_Orig(self, method);
    }

    static void hideGameObjectRecursive(UnityResolve::UnityType::GameObject* gameObject, int current_level, int max_level,
                                 const std::string& prefix = "", std::set<void*>* visited = nullptr, bool debug = true) {
        if (!gameObject || current_level > max_level) {
            return;
        }

        // 防止无限递归
        std::set<void*> local_visited;
        if (!visited) {
            visited = &local_visited;
        }

        void* gameObjectPtr = static_cast<void*>(gameObject);
        if (visited->find(gameObjectPtr) != visited->end()) {
            if (debug) Log::VerboseFmt("%s[L%d] GameObject: %s (ALREADY_VISITED - skipping)", prefix.c_str(), current_level, gameObject->GetName().c_str());
            return;
        }
        visited->insert(gameObjectPtr);

        auto objName = gameObject->GetName();
        auto transform = gameObject->GetTransform();

        // 递归处理子对象 - 深度优先，先隐藏子节点
        if (current_level < max_level && transform) {
            const auto childCount = transform->GetChildCount();
            if (childCount > 0) {
                for (int i = 0; i < childCount; i++) {
                    auto childTransform = transform->GetChild(i);
                    if (childTransform) {
                        auto childGameObject = childTransform->GetGameObject();
                        if (childGameObject && childGameObject != gameObject) { // 避免自引用
                            std::string newPrefix = prefix + "  ";
                            hideGameObjectRecursive(childGameObject, current_level + 1, max_level, newPrefix, visited, debug);
                        }
                    }
                }
            }
        }

        // 隐藏当前GameObject (在隐藏子节点之后)
        if (transform) {
            if (debug) Log::VerboseFmt("%s[L%d] Hiding GameObject: %s", prefix.c_str(), current_level, objName.c_str());
            Il2cppUtils::SetTransformRenderActive(transform, false, objName, debug);
        }
    }

    // return struct value, should use &scene as ptr.
    DEFINE_HOOK(void*, SceneManager_GetSceneByName, (Il2cppString * sceneName, void* method)) {
        Log::DebugFmt("SceneManager_GetSceneByName HOOKED: %s", sceneName->ToString().c_str());
        auto scene = SceneManager_GetSceneByName_Orig(sceneName, method);
        Log::DebugFmt("SceneManager_GetSceneByName HOOKED: %s Finished", sceneName->ToString().c_str());
        static auto Scene_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "Scene");
        static auto Scene_getRootGameObjects = Scene_klass->Get<UnityResolve::Method>("GetRootGameObjects", {});
        static auto Transform_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
        if (sceneName->ToString().starts_with("3d_stage")) {
//            Log::DebugFmt("Scene_getRootGameObjects is at %p", Scene_getRootGameObjects);
            if (Scene_getRootGameObjects) {
                auto gameObjects = Scene_getRootGameObjects->Invoke<UnityResolve::UnityType::Array<UnityResolve::UnityType::GameObject*>*>(&scene);
//                auto gameObjects = Scene_getRootGameObjects(scene);
//                Log::DebugFmt("gameObjects is at %p", gameObjects);
                auto gameObjectsVector = gameObjects->ToVector();
                for (auto object : gameObjectsVector) {
                    auto name = object->GetName();
                    Log::DebugFmt("SceneManager_GetSceneByName game object: %s", name.c_str());
                    switch ((HideLiveStreamSceneItemMode) Config::hideLiveStreamSceneItemsLevel) {
                        case HideLiveStreamSceneItemMode::Normal:
                            if (name.starts_with("Sc")) {
                                hideGameObjectRecursive(object, 0, 12);
                            }
                            break;
                        case HideLiveStreamSceneItemMode::Strong: // 也许可以通过循环实时隐藏，或者是根据阻止timeline显示对应的object
                        case HideLiveStreamSceneItemMode::Ultimate:
                            hideGameObjectRecursive(object, 0, 12);
                            break;
                        default:
                            break;
                    }
                }
            }



        }
        return scene;
    }

    DEFINE_HOOK(void, TimelineCommandReceiver_Awake, (void* self, void* method)) {
        Log::DebugFmt("TimelineCommandReceiver_Awake HOOKED");
        // 可以根据阻止timeline显示对应的object
        if (Config::hideLiveStreamSceneItemsLevel == HideLiveStreamSceneItemMode::Ultimate) return;
        TimelineCommandReceiver_Awake_Orig(self, method);
    }

    DEFINE_HOOK(int32_t, ManagerParams_get_SeatsCount, (Il2cppUtils::Il2CppObject* self, void* method)) {
        auto result = ManagerParams_get_SeatsCount_Orig(self, method);
        Log::DebugFmt("ManagerParams_get_SeatsCount HOOKED: %d", result);
        switch ((HideLiveStreamSceneItemMode) Config::hideLiveStreamSceneItemsLevel) {
            case HideLiveStreamSceneItemMode::Strong: // 也许可以通过循环实时隐藏，或者是根据阻止timeline显示对应的object
            case HideLiveStreamSceneItemMode::Ultimate:
                result = 0;
                break;
            default:
                break;
        }
        return result;
    }
#pragma region draft
    DEFINE_HOOK(void, LiveSceneController_InitializeSceneAsync, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync HOOKED");
        LiveSceneController_InitializeSceneAsync_Orig(self, method);
//        Il2cppUtils::GetClass
//        static auto SceneControllerBase_klass = Il2cppUtils::GetClass("Core.dll", "", "SceneControllerBase`1");
//        static auto view_field = SceneControllerBase_klass->Get<UnityResolve::Field>("_view");
////        Log::DebugFmt("view_field is at: %p", view_field);
//        auto liveSceneView = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Component*>(self, view_field);
//        Log::DebugFmt("liveSceneView is at: %p", liveSceneView);
    }

    DEFINE_HOOK(void*, SceneControllerBase_GetSceneView, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("SceneControllerBase_GetSceneView HOOKED");
        return SceneControllerBase_GetSceneView_Orig(self, method);
    }

    static bool is_first_load = true;

    DEFINE_HOOK(UnityResolve::UnityType::Array<Il2cppString*>*, LiveSceneControllerLogic_FindAssetPaths, (Il2cppUtils::Il2CppObject* self, void* locationsRecord, void* timelinesRecords, void* charactersRecords, void* costumesRecords, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_FindAssetPaths HOOKED");
        auto result = LiveSceneControllerLogic_FindAssetPaths_Orig(self, locationsRecord, timelinesRecords, charactersRecords, costumesRecords, method);
        if (is_first_load) {
            is_first_load = false;
            return result;
        }
        // Convert to vector for safer filtering
//        auto originalVector = result->ToVector();
//        std::vector<Il2cppString*> filteredVector;
//
//        // Filter out stage assets
//        for (auto str : originalVector) {
//            if (str) {
//                Log::DebugFmt("Load asset path: %s", str->ToString().c_str());
//                if (!str->ToString().contains("prop")) {
//                    filteredVector.push_back(str);
//                } else {
//                    Log::DebugFmt("Filtering out prop asset path: %s", str->ToString().c_str());
//                    filteredVector.push_back(str);
//                }
//            }
//        }
//
//        // Create new Array with filtered data
//        static auto Il2cppString_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "String");
//        if (Il2cppString_klass && !filteredVector.empty()) {
//            auto newResult = UnityResolve::UnityType::Array<Il2cppString*>::New(Il2cppString_klass, filteredVector.size());
//            if (newResult) {
//                newResult->Insert(filteredVector.data(), filteredVector.size());
//                return newResult;
//            }
//        }
//
        // Fallback: return original if filtering failed
        return result;
    }

    DEFINE_HOOK(void*, LiveSceneControllerLogic_LoadLocationAssets, (Il2cppUtils::Il2CppObject* self, void* locationsRecord, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_LoadLocationAssets HOOKED");
        return LiveSceneControllerLogic_LoadLocationAssets_Orig(self, locationsRecord, method);
    }


    UnityResolve::UnityType::GameObject* viewObjectCache = nullptr;
    std::atomic<bool> analysisScheduled{false};
    std::atomic<bool> analysisRunning{false};
    std::future<void> analysisFuture;
    
    DEFINE_HOOK(void, LiveSceneControllerLogic_ctor, (Il2cppUtils::Il2CppObject* self, void* param, void* loader, UnityResolve::UnityType::GameObject* view, void* addSceneProvider, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_ctor HOOKED");
        LiveSceneControllerLogic_ctor_Orig(self, param, loader, view, addSceneProvider, method);
        Log::DebugFmt("LiveSceneControllerLogic_ctor HOOKED Finished, ready for following handlers");
        viewObjectCache = view;
    }

    DEFINE_HOOK(void*, SceneChanger_AddSceneAsync, (Il2cppUtils::Il2CppObject* self, Il2cppString* sceneName, bool ignoreEditorSceneManager, void* method)) {
        Log::DebugFmt("SceneChanger_AddSceneAsync HOOKED");
        return SceneChanger_AddSceneAsync_Orig(self, sceneName, ignoreEditorSceneManager, method);
    }

    // lag
    DEFINE_HOOK(void*, LiveSceneController_PrepareChangeSceneAsync, (Il2cppUtils::Il2CppObject* self,void* token, void* method)) {
        Log::DebugFmt("LiveSceneController_PrepareChangeSceneAsync HOOKED");
        return LiveSceneController_PrepareChangeSceneAsync_Orig(self, token, method);
    }

    void printChildren(UnityResolve::UnityType::Transform* obj, const std::string& obj_name) {
        const auto childCount = obj->GetChildCount();
        for (int i = 0;i < childCount; i++) {
            auto child = obj->GetChild(i);
            const auto childName = child->GetName();
            Log::VerboseFmt("%s child: %s", obj_name.c_str(), childName.c_str());
        }
    }

    void printGameObjectComponentsRecursive(UnityResolve::UnityType::GameObject* gameObject, int current_level, int max_level, 
                                           UnityResolve::Class* component_klass = nullptr, const std::string& prefix = "",
                                           std::set<void*>* visited = nullptr) {
        if (!gameObject || current_level > max_level) {
            return;
        }
        
        // 防止无限递归
        std::set<void*> local_visited;
        if (!visited) {
            visited = &local_visited;
        }
        
        void* gameObjectPtr = static_cast<void*>(gameObject);
        if (visited->find(gameObjectPtr) != visited->end()) {
            Log::DebugFmt("%s[L%d] GameObject: %s (ALREADY_VISITED - skipping)", prefix.c_str(), current_level, gameObject->GetName().c_str());
            return;
        }
        visited->insert(gameObjectPtr);
        
        auto objName = gameObject->GetName();
        Log::DebugFmt("%s[L%d] GameObject: %s", prefix.c_str(), current_level, objName.c_str());

        // 使用自定义类型或默认Component类型
        static auto defaultComponent_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
        auto target_klass = component_klass ? component_klass : defaultComponent_klass;
        
        if (target_klass) {
            // 获取指定类型的组件
            auto components = gameObject->GetComponents<UnityResolve::UnityType::Component*>(target_klass);
            Log::DebugFmt("%s  ├─ Components[%s]: %d found", prefix.c_str(), 
                         component_klass ? component_klass->name.c_str() : "Component", components.size());
            
            for (size_t i = 0; i < components.size(); i++) {
                auto component = components[i];
                if (component) {
                    auto componentName = component->GetName();
                    // 获取组件类型信息 - 但不递归进入组件的GameObject
                    Log::DebugFmt("%s  │  ├─ [%d] %s (ptr: %p)", prefix.c_str(), i, componentName.c_str(), component);
                }
            }
        }
        
        // 递归处理子对象 - 优化子对象获取逻辑
        if (current_level < max_level) {
            auto transform = gameObject->GetTransform();
            std::vector<UnityResolve::UnityType::GameObject*> childGameObjects;
            
            // 方法1：直接通过Transform获取子对象
            if (transform) {
                const auto childCount = transform->GetChildCount();
                if (childCount > 0) {
                    Log::DebugFmt("%s  └─ Direct children: %d", prefix.c_str(), childCount);
                    for (int i = 0; i < childCount; i++) {
                        auto childTransform = transform->GetChild(i);
                        if (childTransform) {
                            auto childGameObject = childTransform->GetGameObject();
                            if (childGameObject && childGameObject != gameObject) { // 避免自引用
                                childGameObjects.push_back(childGameObject);
                            }
                        }
                    }
                }
            }
            
            // 方法2：如果没有直接子对象，使用GetComponentsInChildren (但限制层数)
            if (childGameObjects.empty() && current_level < 2) {
                static auto GameObject_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
                if (GameObject_klass) {
                    auto allChildGameObjects = gameObject->GetComponentsInChildren<UnityResolve::UnityType::GameObject*>(GameObject_klass, true);
                    if (allChildGameObjects.size() > 1) { // 大于1因为包含自己
                        Log::DebugFmt("%s  └─ Children via GetComponentsInChildren: %d", prefix.c_str(), std::min(5, (int)(allChildGameObjects.size() - 1)));
                        // 跳过第一个(自己)，添加其余子对象，但限制数量
                        for (size_t i = 1; i < allChildGameObjects.size() && i <= 5; i++) {
                            if (allChildGameObjects[i] && allChildGameObjects[i] != gameObject) {
                                childGameObjects.push_back(allChildGameObjects[i]);
                            }
                        }
                    }
                }
            }
            
            // 递归处理所有找到的子对象
            for (size_t i = 0; i < childGameObjects.size(); i++) {
                auto childGameObject = childGameObjects[i];
                if (childGameObject) {
                    std::string newPrefix = prefix + "  ";
                    printGameObjectComponentsRecursive(childGameObject, current_level + 1, max_level, component_klass, newPrefix, visited);
                }
            }
        }
    }
    void analyzeLiveSceneViewInDepth(UnityResolve::UnityType::Component* liveSceneView, const std::string& prefix = "") {
        if (!liveSceneView) return;

        Log::DebugFmt("%s=== Deep Analysis of LiveSceneView Component ===", prefix.c_str());
        Log::DebugFmt("%sLiveSceneView ptr: %p", prefix.c_str(), liveSceneView);

        // 1. 基本信息
        auto gameObject = liveSceneView->GetGameObject();
        auto transform = liveSceneView->GetTransform();
        Log::DebugFmt("%sGameObject: %s (ptr: %p)", prefix.c_str(),
                      gameObject ? gameObject->GetName().c_str() : "NULL", gameObject);
        Log::DebugFmt("%sTransform: %p", prefix.c_str(), transform);

        // 2. 尝试获取LiveSceneView的类定义和字段
        static auto LiveSceneView_klass = Il2cppUtils::GetClass("Core.dll", "Inspix", "LiveSceneView");
        if (LiveSceneView_klass) {
            Log::DebugFmt("%sLiveSceneView class found: %p", prefix.c_str(), LiveSceneView_klass);

            // 尝试获取一些可能的字段
            std::vector<std::string> possibleFields = {
                    "_view", "view", "sceneView", "mainView",
                    "_transform", "_gameObject", "_renderer",
                    "_camera", "camera", "_canvas", "canvas",
                    "_components", "components", "_children", "children"
            };

            for (const auto& fieldName : possibleFields) {
                auto field = LiveSceneView_klass->Get<UnityResolve::Field>(fieldName);
                if (field) {
                    Log::DebugFmt("%s  Field found: %s (offset: 0x%X, static: %s)", prefix.c_str(),
                                  fieldName.c_str(), field->offset, field->static_field ? "yes" : "no");

                    if (!field->static_field && field->offset > 0) {
                        // 尝试读取字段值
                        try {
                            void* fieldValue = *reinterpret_cast<void**>(
                                    reinterpret_cast<uintptr_t>(liveSceneView) + field->offset
                            );
                            Log::DebugFmt("%s    └─ Value: %p", prefix.c_str(), fieldValue);

                            // 如果字段值看起来像是GameObject或Component
                            if (fieldValue) {
                                auto possibleGameObject = reinterpret_cast<UnityResolve::UnityType::GameObject*>(fieldValue);
                                try {
                                    auto objName = possibleGameObject->GetName();
                                    if (!objName.empty()) {
                                        Log::DebugFmt("%s      └─ GameObject name: %s", prefix.c_str(), objName.c_str());
                                    }
                                } catch (...) {
                                    // 可能不是GameObject，尝试Component
                                    try {
                                        auto possibleComponent = reinterpret_cast<UnityResolve::UnityType::Component*>(fieldValue);
                                        auto compGameObj = possibleComponent->GetGameObject();
                                        if (compGameObj) {
                                            auto compObjName = compGameObj->GetName();
                                            Log::DebugFmt("%s      └─ Component on GameObject: %s", prefix.c_str(), compObjName.c_str());
                                        }
                                    } catch (...) {
                                        // 不是Component
                                    }
                                }
                            }
                        } catch (...) {
                            Log::DebugFmt("%s    └─ Failed to read field value", prefix.c_str());
                        }
                    }
                }
            }
        }

        // 3. 分析LiveSceneView能访问的所有组件
        Log::DebugFmt("%s--- Components accessible from LiveSceneView ---", prefix.c_str());
        auto allComponents = liveSceneView->GetComponentsInChildren<UnityResolve::UnityType::Component*>();
        Log::DebugFmt("%sTotal accessible components: %d", prefix.c_str(), allComponents.size());

        std::map<std::string, int> componentTypeCount;
        std::map<std::string, std::vector<UnityResolve::UnityType::Component*>> componentsByType;

        for (size_t i = 0; i < allComponents.size() && i < 50; i++) { // 限制数量
            auto comp = allComponents[i];
            if (comp) {
                std::string kompName = "Unknown";
                if (comp->Il2CppClass.klass) {
                    kompName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", comp->Il2CppClass.klass);
                }
                componentTypeCount[kompName]++;
                componentsByType[kompName].push_back(comp);
            }
        }

        // 按类型分组显示组件
        for (const auto& pair : componentTypeCount) {
            const auto& typeName = pair.first;
            const auto& count = pair.second;
            Log::DebugFmt("%s  %s: %d instances", prefix.c_str(), typeName.c_str(), count);

            // 显示每个实例所在的GameObject
            for (size_t i = 0; i < componentsByType[typeName].size() && i < 3; i++) {
                auto comp = componentsByType[typeName][i];
                auto compGameObj = comp->GetGameObject();
                auto gameObjName = compGameObj ? compGameObj->GetName() : "NULL";
                Log::DebugFmt("%s    ├─ [%d] on GameObject: %s", prefix.c_str(), i, gameObjName.c_str());
            }
            if (componentsByType[typeName].size() > 3) {
                Log::DebugFmt("%s    └─ ... and %d more", prefix.c_str(),
                              componentsByType[typeName].size() - 3);
            }
        }

        // 4. 分析Transform层级
        if (transform) {
            Log::DebugFmt("%s--- Transform Hierarchy from LiveSceneView ---", prefix.c_str());
            const auto childCount = transform->GetChildCount();
            auto parent = transform->GetParent();

            Log::DebugFmt("%sTransform parent: %s", prefix.c_str(),
                          parent ? (parent->GetGameObject() ? parent->GetGameObject()->GetName().c_str() : "NULL") : "ROOT");
            Log::DebugFmt("%sTransform children: %d", prefix.c_str(), childCount);

            for (int i = 0; i < childCount && i < 10; i++) {
                auto childTransform = transform->GetChild(i);
                if (childTransform) {
                    auto childGameObj = childTransform->GetGameObject();
                    auto childName = childGameObj ? childGameObj->GetName() : "NULL";
                    Log::DebugFmt("%s  Child[%d]: %s", prefix.c_str(), i, childName.c_str());

                    // 显示子对象的组件
                    if (childGameObj) {
                        static auto Component_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
                        if (Component_klass) {
                            auto childComponents = childGameObj->GetComponents<UnityResolve::UnityType::Component*>(Component_klass);
                            Log::DebugFmt("%s    └─ Components: %d", prefix.c_str(), childComponents.size());
                            for (size_t j = 0; j < childComponents.size() && j < 3; j++) {
                                auto childComp = childComponents[j];
                                std::string childCompType = "Unknown";
                                if (childComp && childComp->Il2CppClass.klass) {
                                    childCompType = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", childComp->Il2CppClass.klass);
                                }
                                Log::DebugFmt("%s      ├─ [%d] %s", prefix.c_str(), j, childCompType.c_str());
                            }
                        }
                    }
                }
            }
        }

        Log::DebugFmt("%s=== End Deep Analysis of LiveSceneView ===", prefix.c_str());
    }

    void exploreComponentsInDetail(UnityResolve::UnityType::GameObject* gameObject, const std::string& prefix = "") {
        if (!gameObject) return;
        
        auto objName = gameObject->GetName();
        Log::DebugFmt("%s=== Detailed Component Analysis for: %s ===", prefix.c_str(), objName.c_str());
        
        // 获取所有组件
        static auto Component_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
        if (Component_klass) {
            auto components = gameObject->GetComponents<UnityResolve::UnityType::Component*>(Component_klass);
            Log::DebugFmt("%sTotal components: %d", prefix.c_str(), components.size());
            
            // 检查是否有重复的指针
            std::set<void*> componentPtrs;
            int duplicateCount = 0;
            for (size_t i = 0; i < components.size(); i++) {
                if (componentPtrs.find(components[i]) != componentPtrs.end()) {
                    duplicateCount++;
                } else {
                    componentPtrs.insert(components[i]);
                }
            }
            Log::DebugFmt("%sDuplicate components: %d, Unique: %d", prefix.c_str(), duplicateCount, componentPtrs.size());
            
            for (size_t i = 0; i < components.size(); i++) {
                auto component = components[i];
                if (component) {
                    auto componentName = component->GetName();
                    
                    // 尝试多种方式获取组件的实际类型信息
                    auto componentType = component->GetType();
                    std::string typeName = "Unknown";
                    std::string className = "Unknown";
                    
                    if (componentType) {
                        typeName = componentType->GetFullName();
                        className = componentType->FormatTypeName();
                    }
                    
                    // 尝试从Il2Cpp对象直接获取类型信息
                    void* klass = nullptr;
                    std::string klassName = "Unknown";
                    if (component->Il2CppClass.klass) {
                        klass = component->Il2CppClass.klass;
                        klassName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", klass);
                    }
                    
                    // 检查这个组件是否属于不同的GameObject
                    auto compGameObject = component->GetGameObject();
                    auto compGameObjectName = compGameObject ? compGameObject->GetName() : "NULL";
                    bool isDifferentGameObject = (compGameObject != gameObject);
                    
                    Log::DebugFmt("%s  Component[%d]: %s (ptr: %p)", prefix.c_str(), i, componentName.c_str(), component);
                    Log::DebugFmt("%s    ├─ FullName: %s", prefix.c_str(), typeName.c_str());
                    Log::DebugFmt("%s    ├─ ClassName: %s", prefix.c_str(), className.c_str());
                    Log::DebugFmt("%s    ├─ Il2CppClass: %s (ptr: %p)", prefix.c_str(), klassName.c_str(), klass);
                    Log::DebugFmt("%s    ├─ GameObject: %s %s", prefix.c_str(), compGameObjectName.c_str(), 
                                 isDifferentGameObject ? "(DIFFERENT!)" : "(same)");
                    Log::DebugFmt("%s    ├─ Component==GameObject: %s", prefix.c_str(), 
                                 ((void*)component == gameObject) ? "YES!" : "no");
                    
                    // 如果是LiveSceneView，进行深度分析
                    if (klassName == "LiveSceneView") {
                        Log::DebugFmt("%s    ├─ *** DETECTED LiveSceneView - Starting Deep Analysis ***", prefix.c_str());
                        analyzeLiveSceneViewInDepth(component, prefix + "    ");
                    }
                    
                    // 尝试获取Transform组件信息
                    auto transform = component->GetTransform();
                    if (transform && transform != component) {
                        auto transformGameObj = transform->GetGameObject();
                        auto transformGameObjName = transformGameObj ? transformGameObj->GetName() : "NULL";
                        Log::DebugFmt("%s    ├─ Transform GameObject: %s", prefix.c_str(), transformGameObjName.c_str());
                    }
                    
                    // 尝试获取这个组件的子组件
                    Log::DebugFmt("%s    ├─ Checking for child components...", prefix.c_str());
                    auto childComponents = component->GetComponentsInChildren<UnityResolve::UnityType::Component*>();
                    if (childComponents.size() > 1) { // 大于1因为包含自己
                        Log::DebugFmt("%s    ├─ Child components from %s: %d", prefix.c_str(), klassName.c_str(), childComponents.size() - 1);
                        for (size_t j = 1; j < childComponents.size() && j <= 5; j++) { // 限制显示数量，跳过自己
                            auto childComp = childComponents[j];
                            if (childComp && childComp != component) {
                                auto childCompName = childComp->GetName();
                                auto childCompGameObject = childComp->GetGameObject();
                                auto childCompGameObjectName = childCompGameObject ? childCompGameObject->GetName() : "NULL";
                                
                                // 获取子组件的类型
                                std::string childKlassName = "Unknown";
                                if (childComp->Il2CppClass.klass) {
                                    childKlassName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", childComp->Il2CppClass.klass);
                                }
                                
                                Log::DebugFmt("%s      ├─ [%d] %s (%s) on GameObject: %s", prefix.c_str(), 
                                            j-1, childKlassName.c_str(), childCompName.c_str(), childCompGameObjectName.c_str());
                            }
                        }
                    } else {
                        Log::DebugFmt("%s    ├─ No child components found", prefix.c_str());
                    }
                    
                    // 如果是Transform组件，显示层级信息
                    if (klassName == "Transform") {
                        auto transformComp = reinterpret_cast<UnityResolve::UnityType::Transform*>(component);
                        if (transformComp) {
                            const auto childCount = transformComp->GetChildCount();
                            auto parent = transformComp->GetParent();
                            auto parentName = parent ? (parent->GetGameObject() ? parent->GetGameObject()->GetName() : "NULL") : "ROOT";
                            
                            Log::DebugFmt("%s    ├─ Transform Parent: %s", prefix.c_str(), parentName.c_str());
                            Log::DebugFmt("%s    └─ Transform Children: %d", prefix.c_str(), childCount);
                            
                            for (int k = 0; k < childCount && k < 5; k++) {
                                auto childTransform = transformComp->GetChild(k);
                                if (childTransform) {
                                    auto childGameObj = childTransform->GetGameObject();
                                    auto childObjName = childGameObj ? childGameObj->GetName() : "NULL";
                                    Log::DebugFmt("%s      ├─ Transform Child[%d]: %s", prefix.c_str(), k, childObjName.c_str());
                                }
                            }
                        }
                    }
                    
                    Log::DebugFmt("%s", prefix.c_str()); // 空行分隔
                }
            }
        }
        
        // 尝试获取所有子GameObject
        auto transform = gameObject->GetTransform();
        if (transform) {
            const auto childCount = transform->GetChildCount();
            if (childCount > 0) {
                Log::DebugFmt("%s=== Direct Child GameObjects: %d ===", prefix.c_str(), childCount);
                for (int i = 0; i < childCount && i < 5; i++) {
                    auto childTransform = transform->GetChild(i);
                    if (childTransform) {
                        auto childGameObject = childTransform->GetGameObject();
                        if (childGameObject) {
                            auto childName = childGameObject->GetName();
                            Log::DebugFmt("%s  Child GameObject[%d]: %s", prefix.c_str(), i, childName.c_str());
                            
                            // 递归分析第一级子对象的组件
                            if (i < 2) { // 只分析前两个子对象
                                exploreComponentsInDetail(childGameObject, prefix + "    ");
                            }
                        }
                    }
                }
            }
        }
        
        Log::DebugFmt("%s=== End Detailed Analysis ===", prefix.c_str());
    }

    // 异步执行分析的函数
    void performAsyncAnalysis() {
        if (viewObjectCache) {
            auto gameObject = viewObjectCache;
            auto gameObjectName = gameObject->GetName();
            Log::DebugFmt("Live scene view game object is %s", gameObjectName.c_str());

            // 打印所有组件的递归结构
            Log::DebugFmt("=== Recursive GameObject Components Structure (max 3 levels) ===");
            printGameObjectComponentsRecursive(gameObject, 0, 6);
            Log::DebugFmt("=== End Recursive Structure ===");

            // 详细分析每个组件
            Log::DebugFmt("=== Detailed Component Analysis ===");
            exploreComponentsInDetail(gameObject, "  ");
            Log::DebugFmt("=== End Detailed Component Analysis ===");

            // 可选：打印特定类型的组件
            static auto Transform_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
            if (Transform_klass) {
                Log::DebugFmt("=== Recursive Transform Components Only ===");
                printGameObjectComponentsRecursive(gameObject, 0, 6, Transform_klass);
                Log::DebugFmt("=== End Transform Structure ===");
            }
        }
        analysisScheduled = false;
        analysisRunning = false;
    }

    // 启动异步分析的包装函数
    void startAsyncAnalysis() {
        if (analysisRunning.exchange(true)) {
            return; // 已经在运行中
        }

        try {
            // 使用 std::async 创建异步任务
            analysisFuture = std::async(std::launch::async, []() {
                // 延迟5秒
                std::this_thread::sleep_for(std::chrono::seconds(20));

                // 检查Unity线程状态
                UnityResolve::ThreadAttach();

                Log::DebugFmt("Starting delayed async analysis");
                performAsyncAnalysis();

                UnityResolve::ThreadDetach();
            });
        } catch (const std::exception& e) {
            Log::ErrorFmt("Failed to start async analysis: %s", e.what());
            analysisRunning = false;
        }
    }

    static int hooked_count = 0;
    DEFINE_HOOK(void, LiveSceneController_InitializeSceneAsync_MoveNext, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync_MoveNext HOOKED");
        LiveSceneController_InitializeSceneAsync_MoveNext_Orig(self, method);
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync_MoveNext HOOKED Finished");
//        hooked_count = hooked_count+1;
//        if (hooked_count <= 1) return;
//        // 启动异步非阻塞分析（5秒后执行）
//        if (!analysisScheduled.exchange(true)) {
//            Log::DebugFmt("Starting async analysis in 5 seconds...");
//            startAsyncAnalysis();
//        }
    }

    DEFINE_HOOK(bool, LiveSceneControllerLogic_Initialize_MoveNext, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_Initialize_MoveNext HOOKED");
        auto result = LiveSceneControllerLogic_Initialize_MoveNext_Orig(self, method);
        Log::DebugFmt("LiveSceneControllerLogic_Initialize_MoveNext HOOKED Finished");
        return result;
    }

    DEFINE_HOOK(void*, Scene_GetRootGameObjects_Injected, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("Scene_GetRootGameObjects_Injected HOOKED");
        auto result = Scene_GetRootGameObjects_Injected_Orig(self, method);
        Log::DebugFmt("Scene_GetRootGameObjects_Injected HOOKED Finished");
        return result;
    }

#pragma endregion

    void Install(HookInstaller* hookInstaller) {
        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));
        
        // 👀
        ADD_HOOK(CoverImageCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CoverImageCommandReceiver", "Awake"));
        ADD_HOOK(CharacterVisibleReceiver_SetupExistCharacter, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character", "CharacterVisibleReceiver", "SetupExistCharacter"));

        // 👀 old
        ADD_HOOK(MRS_AppsCoverScreen_SetActiveCoverImage, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.LiveMain", "AppsCoverScreen", "SetActiveCoverImage"));
        ADD_HOOK(CharacterVisibleReceiver_UpdateAvatarVisibility, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.Character", "CharacterVisibleReceiver", "UpdateAvatarVisibility"));
        ADD_HOOK(Hailstorm_AssetDownloadJob_get_UrlBase, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetDownloadJob", "get_UrlBase"));
        ADD_HOOK(Hailstorm_AssetDownloadJob_get_LocalBase, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetDownloadJob", "get_LocalBase"));

        // Experimental: try to bypass iOS AssetBundle build-target validation on Android.
        ADD_HOOK(UnityEngine_Application_get_platform, Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Application::get_platform()"));
        // Hook the real native implementation in libunity via icall resolution (not the il2cpp stub).
        g_unityLoadFromFileInternalAddr = reinterpret_cast<uintptr_t>(Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.AssetBundle::LoadFromFile_Internal(System.String,System.UInt32,System.UInt64)"));
        if (g_unityLoadFromFileInternalAddr != 0) {
            // Pre-scan patch sites before we install the hook (hooking may overwrite the prologue).
            {
                std::lock_guard<std::mutex> lock(g_buildTargetPatchMutex);
                ensure_build_target_patch_sites_locked();
                ensure_build_target_bypass_sites_locked();
            }
            ADD_HOOK(UnityEngine_AssetBundle_LoadFromFile_Internal, reinterpret_cast<void*>(g_unityLoadFromFileInternalAddr));
        } else {
            Log::WarnFmt("Resolve icall failed: UnityEngine.AssetBundle::LoadFromFile_Internal(...) is NULL.");
        }

        ADD_HOOK(FootShadowManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.FootShadow", "FootShadowManipulator", "OnInstantiate"));
        ADD_HOOK(ItemManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.Item", "ItemManipulator", "OnInstantiate"));
        ADD_HOOK(ScenePropManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Operator", "ScenePropManipulator", "OnInstantiate"));
        ADD_HOOK(SceneManager_GetSceneByName, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "SceneManager", "GetSceneByName"));

        ADD_HOOK(TimelineCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "TimelineCommandReceiver", "Awake"));

        ADD_HOOK(ManagerParams_get_SeatsCount, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Audience.IndirectRendering", "ManagerParams", "get_SeatsCount"));
#pragma region draft_hook
//        ADD_HOOK(LiveSceneController_InitializeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "InitializeSceneAsync"));
//        ADD_HOOK(SceneControllerBase_GetSceneView, Il2cppUtils::GetMethodPointer("Core.dll", "", "SceneControllerBase`1", "SetSceneParam"));
//        ADD_HOOK(LiveSceneControllerLogic_FindAssetPaths, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", "FindAssetPaths"));
//        ADD_HOOK(LiveSceneControllerLogic_LoadLocationAssets, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", "LoadLocationAssets"));
//        ADD_HOOK(LiveSceneControllerLogic_ctor, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", ".ctor"));
//        ADD_HOOK(SceneChanger_AddSceneAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "", "SceneChanger", "AddSceneAsync"));
        //        ADD_HOOK(LiveSceneController_PrepareChangeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "PrepareChangeSceneAsync"));

//        Il2cppUtils::MethodInfo* method = nullptr;
//        auto LiveSceneController_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Inspix", "LiveSceneController");
//        if (LiveSceneController_klass) {
//            auto InitializeSceneAsync_klass = Il2cppUtils::find_nested_class_from_name(
//                    LiveSceneController_klass, "<InitializeSceneAsync>d__3");
//            method = Il2cppUtils::GetMethodIl2cpp(InitializeSceneAsync_klass, "MoveNext", 0);
//            if (method) {
//                ADD_HOOK(LiveSceneController_InitializeSceneAsync_MoveNext, method->methodPointer);
//            }
//        }
//        auto LiveSceneControllerLogic_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Inspix", "LiveSceneControllerLogic");
//        if (LiveSceneControllerLogic_klass) {
//            auto Initialize_klass = Il2cppUtils::find_nested_class_from_name(
//                    LiveSceneControllerLogic_klass, "<Initialize>d__12");
//            method = Il2cppUtils::GetMethodIl2cpp(Initialize_klass, "MoveNext", 0);
//            if (method) {
//                ADD_HOOK(LiveSceneControllerLogic_Initialize_MoveNext, method->methodPointer);
//            }
//        }
#pragma endregion

    }
}
