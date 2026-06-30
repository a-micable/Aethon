#include "aethon/storage/archive_export_writers.hpp"

#include "aethon/protocol/inspector.hpp"

#include <ostream>
#include <sstream>

namespace aethon::storage {
namespace {

void write_csv_record_header(std::ostream& out, const ArchiveExportWriterOptions& options) {
    bool first = true;
    auto column = [&](const char* name) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << name;
    };
    if (options.include_archive_path) {
        column("archive_path");
    }
    if (options.include_offsets) {
        column("offset");
    }
    column("capture_time_ns");
    column("device");
    column("sequence");
    if (options.include_packet_kind) {
        column("kind");
    }
    if (options.include_payload_size) {
        column("payload_size");
    }
    out << "\n";
}

void write_csv_record(std::ostream& out,
                      const ArchiveRecord& record,
                      const ArchiveExportWriterOptions& options,
                      const std::filesystem::path& path) {
    bool first = true;
    auto field = [&](const auto& value) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << value;
    };
    if (options.include_archive_path) {
        field(path.string());
    }
    if (options.include_offsets) {
        field(record.offset);
    }
    field(record.capture_time_ns);
    field(record.packet.device);
    field(record.packet.sequence);
    if (options.include_packet_kind) {
        field(protocol::packet_kind_name(record.packet.kind));
    }
    if (options.include_payload_size) {
        field(record.packet.payload.size());
    }
    out << "\n";
}

void write_csv_ref(std::ostream& out,
                   const ArchiveCollectionRecordRef& ref,
                   const ArchiveExportWriterOptions& options) {
    bool first = true;
    auto field = [&](const auto& value) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << value;
    };
    if (options.include_archive_path) {
        field(ref.path.string());
    }
    if (options.include_offsets) {
        field(ref.offset);
    }
    field(ref.capture_time_ns);
    field(ref.device);
    field(ref.sequence);
    if (options.include_packet_kind) {
        field("unknown");
    }
    if (options.include_payload_size) {
        field(ref.payload_size);
    }
    out << "\n";
}

void write_manifest_text(std::ostream& out,
                         const ArchiveManifest& manifest,
                         const std::filesystem::path& path) {
    out << "manifest";
    if (!path.empty()) {
        out << " path=" << path.string();
    }
    out << " records=" << manifest.summary.record_count
        << " first_time_ns=" << manifest.summary.first_time_ns
        << " last_time_ns=" << manifest.summary.last_time_ns
        << "\n";
    for (const auto& device : manifest.devices) {
        out << "device " << device.device
            << " records=" << device.record_count
            << " payload_bytes=" << device.total_payload_bytes
            << "\n";
    }
}

void write_manifest_csv(std::ostream& out,
                        const ArchiveManifest& manifest,
                        const ArchiveExportWriterOptions& options,
                        const std::filesystem::path& path) {
    if (options.include_header) {
        if (options.include_archive_path) {
            out << "archive_path,";
        }
        out << "device,records,first_time_ns,last_time_ns,total_payload_bytes\n";
    }
    for (const auto& device : manifest.devices) {
        if (options.include_archive_path) {
            out << path.string() << ",";
        }
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

std::string bool_string(bool value) {
    return value ? "true" : "false";
}

} // namespace

ArchiveRecordExportWriter::ArchiveRecordExportWriter(ArchiveExportWriterOptions options)
    : options_(options) {}

void ArchiveRecordExportWriter::write_header(std::ostream& out) const {
    if (!options_.include_header || options_.format != ExportFormat::csv) {
        return;
    }
    write_csv_record_header(out, options_);
}

void ArchiveRecordExportWriter::write_record(std::ostream& out,
                                             const ArchiveRecord& record,
                                             const std::filesystem::path& path) const {
    switch (options_.format) {
    case ExportFormat::csv:
        write_csv_record(out, record, options_, path);
        break;
    case ExportFormat::json_lines:
        out << render_record_json_line(record, options_, path) << "\n";
        break;
    case ExportFormat::text_summary:
        out << "record path=" << path.string()
            << " offset=" << record.offset
            << " capture_time_ns=" << record.capture_time_ns
            << " device=" << record.packet.device
            << " sequence=" << record.packet.sequence
            << " kind=" << protocol::packet_kind_name(record.packet.kind)
            << " payload_size=" << record.packet.payload.size()
            << "\n";
        break;
    }
}

void ArchiveRecordExportWriter::write_ref(std::ostream& out,
                                          const ArchiveCollectionRecordRef& ref) const {
    switch (options_.format) {
    case ExportFormat::csv:
        write_csv_ref(out, ref, options_);
        break;
    case ExportFormat::json_lines:
        out << render_ref_json_line(ref, options_) << "\n";
        break;
    case ExportFormat::text_summary:
        out << "ref path=" << ref.path.string()
            << " ordinal=" << ref.archive_ordinal
            << " offset=" << ref.offset
            << " capture_time_ns=" << ref.capture_time_ns
            << " device=" << ref.device
            << " sequence=" << ref.sequence
            << " payload_size=" << ref.payload_size
            << "\n";
        break;
    }
}

void ArchiveRecordExportWriter::write_collection(std::ostream& out,
                                                 const ArchiveCollection& collection) const {
    write_header(out);
    for (const auto& ref : collection.records) {
        write_ref(out, ref);
    }
}

void ArchiveRecordExportWriter::write_search_result(std::ostream& out,
                                                    const CollectionSearchResult& result) const {
    write_header(out);
    if (!result.records.empty()) {
        for (const auto& record : result.records) {
            write_record(out, record);
        }
        return;
    }
    for (const auto& ref : result.refs) {
        write_ref(out, ref);
    }
}

ArchiveSummaryExportWriter::ArchiveSummaryExportWriter(ArchiveExportWriterOptions options)
    : options_(options) {}

void ArchiveSummaryExportWriter::write_manifest(std::ostream& out,
                                                const ArchiveManifest& manifest,
                                                const std::filesystem::path& path) const {
    switch (options_.format) {
    case ExportFormat::csv:
        write_manifest_csv(out, manifest, options_, path);
        break;
    case ExportFormat::json_lines:
        out << render_manifest_json_line(manifest, options_, path) << "\n";
        break;
    case ExportFormat::text_summary:
        write_manifest_text(out, manifest, path);
        break;
    }
}

void ArchiveSummaryExportWriter::write_collection(std::ostream& out,
                                                  const ArchiveCollection& collection) const {
    for (const auto& archive : collection.archives) {
        write_manifest(out, archive.manifest, archive.path);
    }
}

void ArchiveSummaryExportWriter::write_sample(std::ostream& out, const RecordSample& sample) const {
    if (options_.format == ExportFormat::json_lines) {
        for (const auto& item : sample.refs) {
            out << "{\"sample_rank\":" << item.sample_rank
                << ",\"reason\":\"" << escape_json_string(item.reason)
                << "\",\"ref\":" << render_ref_json_line(item.ref, options_)
                << "}\n";
        }
        return;
    }
    out << render_record_sample(sample);
}

void ArchiveSummaryExportWriter::write_manifest_diff(std::ostream& out,
                                                     const ManifestDiff& diff) const {
    if (options_.format == ExportFormat::json_lines) {
        for (const auto& entry : diff.entries) {
            out << "{\"kind\":\"" << manifest_diff_kind_name(entry.kind)
                << "\",\"key\":\"" << escape_json_string(entry.key)
                << "\",\"record_delta\":" << entry.record_delta
                << ",\"payload_delta\":" << entry.payload_delta
                << ",\"before\":\"" << escape_json_string(entry.before)
                << "\",\"after\":\"" << escape_json_string(entry.after)
                << "\"}\n";
        }
        return;
    }
    out << render_manifest_diff(diff);
}

std::string export_format_name(ExportFormat format) {
    switch (format) {
    case ExportFormat::csv:
        return "csv";
    case ExportFormat::json_lines:
        return "json_lines";
    case ExportFormat::text_summary:
        return "text_summary";
    }
    return "unknown";
}

std::string escape_json_string(const std::string& value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }
    return out.str();
}

std::string render_record_json_line(const ArchiveRecord& record,
                                    const ArchiveExportWriterOptions& options,
                                    const std::filesystem::path& path) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    auto comma = [&]() {
        if (!first) {
            out << ",";
        }
        first = false;
    };
    if (options.include_archive_path) {
        comma();
        out << "\"archive_path\":\"" << escape_json_string(path.string()) << "\"";
    }
    if (options.include_offsets) {
        comma();
        out << "\"offset\":" << record.offset;
    }
    comma();
    out << "\"capture_time_ns\":" << record.capture_time_ns;
    comma();
    out << "\"device\":" << record.packet.device;
    comma();
    out << "\"sequence\":" << record.packet.sequence;
    if (options.include_packet_kind) {
        comma();
        out << "\"kind\":\"" << protocol::packet_kind_name(record.packet.kind) << "\"";
    }
    if (options.include_payload_size) {
        comma();
        out << "\"payload_size\":" << record.packet.payload.size();
    }
    out << "}";
    return out.str();
}

std::string render_ref_json_line(const ArchiveCollectionRecordRef& ref,
                                 const ArchiveExportWriterOptions& options) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    auto comma = [&]() {
        if (!first) {
            out << ",";
        }
        first = false;
    };
    if (options.include_archive_path) {
        comma();
        out << "\"archive_path\":\"" << escape_json_string(ref.path.string()) << "\"";
    }
    comma();
    out << "\"archive_ordinal\":" << ref.archive_ordinal;
    if (options.include_offsets) {
        comma();
        out << "\"offset\":" << ref.offset;
    }
    comma();
    out << "\"capture_time_ns\":" << ref.capture_time_ns;
    comma();
    out << "\"device\":" << ref.device;
    comma();
    out << "\"sequence\":" << ref.sequence;
    if (options.include_payload_size) {
        comma();
        out << "\"payload_size\":" << ref.payload_size;
    }
    out << "}";
    return out.str();
}

std::string render_manifest_json_line(const ArchiveManifest& manifest,
                                      const ArchiveExportWriterOptions& options,
                                      const std::filesystem::path& path) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    auto comma = [&]() {
        if (!first) {
            out << ",";
        }
        first = false;
    };
    if (options.include_archive_path) {
        comma();
        out << "\"archive_path\":\"" << escape_json_string(path.string()) << "\"";
    }
    comma();
    out << "\"records\":" << manifest.summary.record_count;
    comma();
    out << "\"first_time_ns\":" << manifest.summary.first_time_ns;
    comma();
    out << "\"last_time_ns\":" << manifest.summary.last_time_ns;
    comma();
    out << "\"warnings\":" << manifest.warnings.size();
    comma();
    out << "\"devices\":[";
    for (std::size_t i = 0; i < manifest.devices.size(); ++i) {
        const auto& device = manifest.devices[i];
        if (i != 0) {
            out << ",";
        }
        out << "{\"device\":" << device.device
            << ",\"records\":" << device.record_count
            << ",\"payload_bytes\":" << device.total_payload_bytes
            << "}";
    }
    out << "]";
    comma();
    out << "\"ok\":" << bool_string(manifest.warnings.empty());
    out << "}";
    return out.str();
}

} // namespace aethon::storage
