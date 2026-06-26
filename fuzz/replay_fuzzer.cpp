#include "aethon/replay/replay_engine.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) { auto path = std::filesystem::temp_directory_path() / "aethon_replay_fuzz.ath"; { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size)); } try { aethon::storage::ArchiveReader reader(path); aethon::replay::ReplayEngine engine({3.0, 0, 0, false}); (void)engine.run(reader, [](const aethon::storage::ArchiveRecord&) {}); } catch (...) {} std::filesystem::remove(path); return 0; }
