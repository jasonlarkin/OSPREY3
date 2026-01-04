// Seed generator for the ConfSearchAStar libFuzzer target.
//
// This writes a few tiny, deterministic inputs that exercise different config-bit modes
// (prefer-fast, cross-check, extreme, tie-heavy) and a couple of different conf-space shapes.
//
// Usage:
//   gen_fuzz_seeds_conf_search_astar <out_dir>
//
// The fuzzer input format is documented in fuzz_conf_search_astar.cpp.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool write_one(const fs::path& out_dir, const std::string& name, const std::vector<std::uint8_t>& bytes) {
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::cerr << "[seedgen] failed to create dir: " << out_dir.string() << " ec=" << ec.message() << "\n";
        return false;
    }

    const fs::path out = out_dir / name;
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        std::cerr << "[seedgen] failed to open: " << out.string() << "\n";
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        std::cerr << "[seedgen] failed to write: " << out.string() << "\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2 || argv[1] == nullptr) {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "gen_fuzz_seeds_conf_search_astar") << " <out_dir>\n";
        return 2;
    }

    const fs::path out_dir(argv[1]);

    // Notes:
    // - byte0 selects num_positions: 2 + (byte0 % 9)
    // - byte1 is cfg bits (prefer_fast, cross_check, extreme, tie_heavy)
    // - then num_positions bytes for num_confs (1 + (b % 8))
    // - remaining bytes are signed energy bytes (i8); missing bytes read as 0 (still valid)
    //
    // Keep all seeds >= 8 bytes (harness minimum).
    bool ok = true;

    // 2 positions, 2 confs each, baseline, mild energies (zeros by default).
    ok &= write_one(out_dir, "seed_2pos_2x2_baseline.bin",
                    {0x00, 0x00, 0x01, 0x01, 0, 0, 0, 0});

    // 2 positions, 2 confs each, prefer fast.
    ok &= write_one(out_dir, "seed_2pos_2x2_fast.bin",
                    {0x00, 0x01, 0x01, 0x01, 0, 0, 0, 0});

    // 2 positions, 2 confs each, cross-check baseline vs fast.
    ok &= write_one(out_dir, "seed_2pos_2x2_cross_check.bin",
                    {0x00, 0x02, 0x01, 0x01, 0, 0, 0, 0});

    // 4 positions, 4 confs each (256 total), cross-check + extreme energies.
    ok &= write_one(out_dir, "seed_4pos_4x4_extreme_cross_check.bin",
                    {0x02, 0x06, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x80, 0x7f, 0x80, 0});

    // 6 positions, mixed confs, tie-heavy (many equal energies).
    ok &= write_one(out_dir, "seed_6pos_tie_heavy.bin",
                    {0x04, 0x08, 0x01, 0x02, 0x01, 0x00, 0x01, 0x00, 0, 0, 0, 0});

    // 10 positions, 1 conf each (degenerate but legal), prefer-fast + tie-heavy + extreme.
    // byte0=0x08 => 2+(8%9)=10. num_confs bytes all 0 => 1 conf per pos.
    std::vector<std::uint8_t> tenpos;
    tenpos.reserve(2 + 10);
    tenpos.push_back(0x08);
    tenpos.push_back(0x0D); // 0b1101: prefer_fast + extreme + tie_heavy
    for (int i = 0; i < 10; ++i) tenpos.push_back(0x00);
    ok &= write_one(out_dir, "seed_10pos_all1.bin", tenpos);

    if (!ok) {
        return 1;
    }

    std::cout << "[seedgen] wrote seeds to: " << out_dir.string() << "\n";
    return 0;
}

