// Seed generator for the PartitionFunction libFuzzer target.
//
// Usage:
//   gen_fuzz_seeds_partition_function <out_dir>
//
// The fuzzer input format is documented in fuzz_partition_function.cpp.

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
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "gen_fuzz_seeds_partition_function") << " <out_dir>\n";
        return 2;
    }

    const fs::path out_dir(argv[1]);

    // Notes:
    // - byte0 selects num_positions: 2 + (byte0 % 7)
    // - byte1 is cfg bits:
    //   bit0 prefer_fast, bit1 use_gd, bit2 eps_zero, bit3 extreme, bit4 tie_heavy, bit5 inject_nan, bit6 oracle_check
    // - next num_positions bytes for num_confs (1 + (b % 6))
    // - remaining bytes are signed energy bytes (i8)
    //
    // Keep all seeds >= 8 bytes (harness minimum).
    bool ok = true;

    // A* baseline, epsilon=0.2, mild energies, small space.
    ok &= write_one(out_dir, "seed_astar_baseline_eps02.bin",
                    {0x00, 0x00, 0x01, 0x01, 0, 0, 0, 0});

    // A* fast preferred, epsilon=0.2, cross-check oracle containment.
    ok &= write_one(out_dir, "seed_astar_fast_oracle_check.bin",
                    {0x00, 0x41, 0x01, 0x01, 0, 0, 0, 0});

    // GradientDescent, epsilon=0.2, tie-heavy energies.
    ok &= write_one(out_dir, "seed_gd_tie_heavy.bin",
                    {0x01, 0x12, 0x02, 0x02, 0x02, 0, 0, 0});

    // A* baseline, epsilon=0 (forces full enumeration through A* path), tiny space.
    ok &= write_one(out_dir, "seed_astar_eps0_tiny.bin",
                    {0x00, 0x04, 0x01, 0x01, 0, 0, 0, 0});

    // NaN injection policy.
    ok &= write_one(out_dir, "seed_nan_policy.bin",
                    {0x00, 0x20, 0x01, 0x01, 0, 0, 0, 0});

    // Float instantiation: A* fast + oracle containment (bit7 set).
    ok &= write_one(out_dir, "seed_float_astar_fast_oracle_check.bin",
                    {0x00, 0xC1, 0x01, 0x01, 0, 0, 0, 0});

    // Extreme energies, 6 positions, mixed confs.
    ok &= write_one(out_dir, "seed_extreme_6pos.bin",
                    {0x04, 0x08, 0x01, 0x02, 0x01, 0x00, 0x01, 0x00, 0x7f, 0x80, 0});

    if (!ok) return 1;
    std::cout << "[seedgen] wrote seeds to: " << out_dir.string() << "\n";
    return 0;
}

