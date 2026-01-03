#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void write_if_missing(const fs::path& dir, const std::string& name, const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(dir);
    const fs::path path = dir / name;
    if (fs::exists(path)) return;

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

static void append_be_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

static void append_be_f64(std::vector<std::uint8_t>& out, double d) {
    static_assert(sizeof(double) == 8);
    std::uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(bits));
    out.push_back(static_cast<std::uint8_t>((bits >> 56) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 48) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 40) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 32) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(bits & 0xFF));
}

// Generates a few tiny valid-ish EnergyMatrix binaries (Java DataOutputStream layout, big-endian).
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const fs::path corpus_dir = argv[1];

    // seed0: constTerm=0.0, numPositions=0
    {
        std::vector<std::uint8_t> b;
        append_be_f64(b, 0.0);
        append_be_u32(b, 0);
        write_if_missing(corpus_dir, "seed0_pos0.bin", b);
    }

    // seed1: constTerm=0.0, numPositions=1, numConfs=[1], oneBody=[0.0], pairwise=[]
    {
        std::vector<std::uint8_t> b;
        append_be_f64(b, 0.0);
        append_be_u32(b, 1);
        append_be_u32(b, 1);
        append_be_f64(b, 0.0);
        write_if_missing(corpus_dir, "seed1_pos1_conf1.bin", b);
    }

    // seed2: constTerm=0.0, numPositions=2, numConfs=[1,1], oneBody=[0.0,0.0], pairwise=[0.0]
    {
        std::vector<std::uint8_t> b;
        append_be_f64(b, 0.0);
        append_be_u32(b, 2);
        append_be_u32(b, 1);
        append_be_u32(b, 1);
        append_be_f64(b, 0.0);
        append_be_f64(b, 0.0);
        append_be_f64(b, 0.0);
        write_if_missing(corpus_dir, "seed2_pos2_conf1_1.bin", b);
    }

    return 0;
}


