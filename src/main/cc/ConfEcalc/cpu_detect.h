#ifndef CONFECALC_CPU_DETECT_H
#define CONFECALC_CPU_DETECT_H

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace osprey::cpu_detect {

    // CPU capability flags
    struct CPUCapabilities {
        bool has_avx2 = false;
        bool has_avx512f = false;
        bool has_avx512dq = false;
        bool has_fma = false;
    };

    // Detect CPU capabilities using cpuid
    // This is called once at runtime to determine which SIMD version to use
    inline CPUCapabilities detect_cpu_capabilities() {
        CPUCapabilities caps;
        
        #ifdef __x86_64__
        // Check for AVX2 and FMA (CPUID level 7, EBX)
        uint32_t eax, ebx, ecx, edx;
        
        // Get highest function number
        asm volatile ("cpuid"
                     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                     : "a" (0x7), "c" (0));
        
        // Check AVX2 (bit 5 of EBX)
        caps.has_avx2 = (ebx & (1u << 5)) != 0;
        
        // Check AVX512F (bit 16 of EBX)
        caps.has_avx512f = (ebx & (1u << 16)) != 0;
        
        // Check AVX512DQ (bit 17 of EBX)
        caps.has_avx512dq = (ebx & (1u << 17)) != 0;
        
        // Check FMA (CPUID level 1, ECX bit 12)
        asm volatile ("cpuid"
                     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                     : "a" (0x1));
        caps.has_fma = (ecx & (1u << 12)) != 0;
        
        #elif defined(__linux__)
        // Fallback: Parse /proc/cpuinfo on Linux
        // This is less efficient but works without inline assembly
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "flags")) {
                    caps.has_avx2 = strstr(line, "avx2") != nullptr;
                    caps.has_avx512f = strstr(line, "avx512f") != nullptr;
                    caps.has_avx512dq = strstr(line, "avx512dq") != nullptr;
                    caps.has_fma = strstr(line, "fma") != nullptr;
                    break;
                }
            }
            fclose(f);
        }
        #endif
        
        return caps;
    }

} // namespace osprey::cpu_detect

#endif // CONFECALC_CPU_DETECT_H

