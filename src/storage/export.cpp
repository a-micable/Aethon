#include "aethon/storage/export.hpp"

#include "aethon/protocol/inspector.hpp"

#include <sstream>

namespace aethon::storage {

void export_archive_records(std::ostream& out,
                            const std::filesystem::path& path,
                            const ExportOptions& options) {
    if (options.include_header) {
        out << render_record_csv_header(options) << "\n";
    }
    ArchiveReader reader(path);
    while (auto record = reader.next()) {
        out << render_record_csv_row(*record, options) << "\n";
    }
}

void export_manifest_devices(std::ostream& out,
                             const ArchiveManifest& manifest,
                             const ExportOptions& options) {
    if (options.include_header) {
        out << "device,records,first_time_ns,last_time_ns,total_payload_bytes\n";
    }
    (void)options;
    for (const auto& device : manifest.devices) {
        out << device.device
            << ","
            << device.record_count
            << ","
            << device.first_time_ns
            << ","
            << device.last_time_ns
            << ","
            << device.total_payload_bytes
            << "\n";
    }
}

std::string render_record_csv_header(const ExportOptions& options) {
    std::ostringstream out;
    out << "capture_time_ns,device,sequence";
    if (options.include_kind) {
        out << ",kind";
    }
    if (options.include_payload_size) {
        out << ",payload_size";
    }
    return out.str();
}

std::string render_record_csv_row(const ArchiveRecord& record,
                                  const ExportOptions& options) {
    std::ostringstream out;
    out << record.capture_time_ns
        << ","
        << record.packet.device
        << ","
        << record.packet.sequence;
    if (options.include_kind) {
        out << ","
            << protocol::packet_kind_name(record.packet.kind);
    }
    if (options.include_payload_size) {
        out << ","
            << record.packet.payload.size();
    }
    return out.str();
}

} // namespace aethon::storage
