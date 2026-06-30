#include "test_harness.hpp"

#include "aethon/control/ack_tracker.hpp"
#include "aethon/control/backpressure_controller.hpp"
#include "aethon/control/command_journal.hpp"
#include "aethon/control/device_limiter.hpp"
#include "aethon/protocol/packet_builder.hpp"
#include "aethon/runtime/batching_policy.hpp"
#include "aethon/runtime/ingest_queue.hpp"
#include "aethon/runtime/scheduler.hpp"

#include <string>

namespace {

aethon::protocol::Packet packet(std::uint64_t device,
                                std::uint32_t sequence,
                                std::initializer_list<std::uint8_t> payload) {
    auto pkt = aethon::protocol::make_observation(device, 1000 + sequence, sequence, {});
    pkt.payload.assign(payload.begin(), payload.end());
    return pkt;
}

aethon::runtime::IngestEnvelope envelope(std::uint64_t device,
                                         std::uint32_t sequence,
                                         std::uint64_t arrival,
                                         aethon::runtime::IngestPriority priority) {
    aethon::runtime::IngestEnvelope env;
    env.packet = packet(device, sequence, {1, 2, 3});
    env.arrival_time_ns = arrival;
    env.deadline_ns = arrival + 1000;
    env.priority = priority;
    env.source = "test";
    return env;
}

} // namespace

AETHON_TEST(ingest_queue_eviction_and_priority_drain_are_deterministic) {
    aethon::runtime::IngestQueueConfig config;
    config.max_packets = 2;
    config.overflow_policy = aethon::runtime::IngestOverflowPolicy::drop_lowest_priority;
    config.drain_mode = aethon::runtime::IngestDrainMode::highest_priority_first;

    aethon::runtime::IngestQueue queue(config);
    auto first = queue.push(envelope(10, 1, 10, aethon::runtime::IngestPriority::low));
    auto second = queue.push(envelope(11, 1, 11, aethon::runtime::IngestPriority::normal));
    auto third = queue.push(envelope(12, 1, 12, aethon::runtime::IngestPriority::critical));

    AETHON_REQUIRE(first.status == aethon::runtime::IngestAdmissionStatus::accepted);
    AETHON_REQUIRE(second.status == aethon::runtime::IngestAdmissionStatus::accepted);
    AETHON_REQUIRE(third.evicted.has_value());
    AETHON_REQUIRE(third.evicted->device == 10);

    auto drained = queue.drain(2);
    AETHON_REQUIRE(drained.packets.size() == 2);
    AETHON_REQUIRE(drained.packets[0].packet.device == 12);
    AETHON_REQUIRE(drained.packets[1].packet.device == 11);
    AETHON_REQUIRE(queue.stats().evicted == 1);
    AETHON_REQUIRE(aethon::runtime::render_ingest_queue_stats(queue.stats()).find("evicted") != std::string::npos);
}

AETHON_TEST(batching_policy_closes_on_limits_and_priority) {
    aethon::runtime::BatchingPolicyConfig config;
    config.max_packets = 2;
    config.priority_flush_threshold = aethon::runtime::IngestPriority::critical;
    aethon::runtime::BatchingPolicy policy(config);

    auto ready = policy.offer(envelope(20, 1, 100, aethon::runtime::IngestPriority::normal), 100);
    AETHON_REQUIRE(ready.empty());
    ready = policy.offer(envelope(20, 2, 101, aethon::runtime::IngestPriority::normal), 101);
    AETHON_REQUIRE(ready.size() == 1);
    AETHON_REQUIRE(ready.front().close_reason == aethon::runtime::BatchCloseReason::max_packets);

    ready = policy.offer(envelope(20, 3, 102, aethon::runtime::IngestPriority::critical), 102);
    AETHON_REQUIRE(ready.size() == 1);
    AETHON_REQUIRE(ready.front().close_reason == aethon::runtime::BatchCloseReason::priority_flush);
    AETHON_REQUIRE(policy.stats().batches_closed == 2);
}

AETHON_TEST(backpressure_controller_delays_and_downsamples_by_pressure) {
    aethon::control::BackpressureConfig config;
    config.queue_capacity_packets = 10;
    config.payload_capacity_bytes = 1000;
    config.min_delay_ns = 10;
    aethon::control::BackpressureController controller(config);

    aethon::control::PressureSample throttle;
    throttle.time_ns = 100;
    throttle.queued_packets = 8;
    controller.observe_sample(throttle);
    auto delayed = controller.decide(packet(30, 1, {1}), 100);
    AETHON_REQUIRE(delayed.action == aethon::control::AdmissionAction::delay);
    AETHON_REQUIRE(delayed.delay_ns >= 10);

    aethon::control::PressureSample shed;
    shed.time_ns = 200;
    shed.queued_packets = 9;
    controller.observe_sample(shed);
    auto downsampled = controller.decide(packet(30, 2, {1}), 1000);
    AETHON_REQUIRE(downsampled.action == aethon::control::AdmissionAction::downsample);
    AETHON_REQUIRE(downsampled.downsample_factor >= 2);
    AETHON_REQUIRE(aethon::control::render_backpressure_snapshot(controller.snapshot()).find("shed") != std::string::npos);
}

AETHON_TEST(command_journal_dispatches_retries_and_acks_commands) {
    aethon::control::CommandJournalConfig config;
    config.retry.initial_delay_ns = 100;
    config.retry.max_attempts = 3;
    aethon::control::CommandJournal journal(config);

    auto appended = journal.append(aethon::control::make_start_stream_command(40, 100, {9}));
    AETHON_REQUIRE(appended.appended);

    auto due = journal.due(100, 4);
    AETHON_REQUIRE(due.size() == 1);
    AETHON_REQUIRE(due.front().packet.kind == aethon::protocol::PacketKind::control);
    AETHON_REQUIRE(due.front().attempt == 1);

    aethon::control::CommandAck busy;
    busy.command_id = appended.id;
    busy.device = 40;
    busy.status = aethon::control::AckStatus::busy;
    busy.time_ns = 120;
    auto busy_result = journal.acknowledge(busy);
    AETHON_REQUIRE(busy_result.matched);
    AETHON_REQUIRE(busy_result.state == aethon::control::CommandState::pending);

    auto retry = journal.due(320, 4);
    AETHON_REQUIRE(retry.size() == 1);
    AETHON_REQUIRE(retry.front().attempt == 2);

    aethon::control::CommandAck ack;
    ack.command_id = appended.id;
    ack.device = 40;
    ack.status = aethon::control::AckStatus::accepted;
    ack.time_ns = 350;
    auto acked = journal.acknowledge(ack);
    AETHON_REQUIRE(acked.state == aethon::control::CommandState::acknowledged);
    AETHON_REQUIRE(journal.stats().acknowledged == 1);
}

AETHON_TEST(runtime_scheduler_prefers_deadlines_and_tracks_retries) {
    aethon::runtime::RuntimeScheduler scheduler;

    aethon::runtime::SchedulerTask slow;
    slow.kind = aethon::runtime::SchedulerTaskKind::batch;
    slow.device = 1;
    slow.enqueue_time_ns = 100;
    slow.ready_time_ns = 100;
    slow.deadline_ns = 1000;
    slow.priority = aethon::runtime::IngestPriority::normal;
    slow.label = "slow";

    aethon::runtime::SchedulerTask urgent = slow;
    urgent.device = 2;
    urgent.deadline_ns = 150;
    urgent.priority = aethon::runtime::IngestPriority::high;
    urgent.label = "urgent";

    auto slow_id = scheduler.enqueue(slow);
    auto urgent_id = scheduler.enqueue(urgent);
    AETHON_REQUIRE(slow_id != 0);
    AETHON_REQUIRE(urgent_id != 0);

    auto lease = scheduler.lease_next(125);
    AETHON_REQUIRE(lease.has_value());
    AETHON_REQUIRE(lease->task.id == urgent_id);
    AETHON_REQUIRE(lease->reason == aethon::runtime::SchedulerPickReason::deadline);

    aethon::runtime::SchedulerCompletion failed;
    failed.id = urgent_id;
    failed.success = false;
    failed.retry_ready_time_ns = 200;
    failed.note = "retry";
    AETHON_REQUIRE(scheduler.complete(failed));
    AETHON_REQUIRE(scheduler.stats().retried == 1);
}

AETHON_TEST(device_limiter_enforces_locks_and_token_refill) {
    aethon::control::DeviceLimiterConfig config;
    config.default_limit.burst = 2;
    config.default_limit.refill_per_second = 2;
    aethon::control::DeviceLimiter limiter(config);
    limiter.register_device(50, config.default_limit);

    auto lease = limiter.acquire({50, aethon::control::DeviceLeaseMode::exclusive, 100, 1000, "maintenance"});
    AETHON_REQUIRE(lease.has_value());
    auto locked = limiter.consume(50, 1, 100);
    AETHON_REQUIRE(locked.decision == aethon::control::DeviceLimitDecision::locked);
    AETHON_REQUIRE(limiter.release(lease->id));

    auto first = limiter.consume(50, 1, 200);
    auto second = limiter.consume(50, 1, 200);
    auto limited = limiter.consume(50, 1, 200);
    AETHON_REQUIRE(first.decision == aethon::control::DeviceLimitDecision::allowed);
    AETHON_REQUIRE(second.decision == aethon::control::DeviceLimitDecision::allowed);
    AETHON_REQUIRE(limited.decision == aethon::control::DeviceLimitDecision::rate_limited);

    auto refilled = limiter.consume(50, 1, 700'000'200);
    AETHON_REQUIRE(refilled.decision == aethon::control::DeviceLimitDecision::allowed);
    AETHON_REQUIRE(aethon::control::render_device_limiter_snapshot(limiter.snapshot()).find("device: 50") != std::string::npos);
}

AETHON_TEST(ack_tracker_matches_duplicates_and_times_out_expectations) {
    aethon::control::AckTracker tracker;
    aethon::control::AckExpectation expectation;
    expectation.command_id = 77;
    expectation.device = 60;
    expectation.created_time_ns = 100;
    expectation.deadline_ns = 500;
    expectation.label = "configure";
    AETHON_REQUIRE(tracker.expect(expectation));

    aethon::control::CommandAck ack;
    ack.command_id = 77;
    ack.device = 60;
    ack.status = aethon::control::AckStatus::accepted;
    ack.time_ns = 200;
    auto matched = tracker.observe(ack);
    AETHON_REQUIRE(matched.kind == aethon::control::AckObservationKind::matched);
    AETHON_REQUIRE(tracker.find(77)->state == aethon::control::AckExpectationState::satisfied);

    auto duplicate = tracker.observe(ack);
    AETHON_REQUIRE(duplicate.kind == aethon::control::AckObservationKind::duplicate);

    aethon::control::CommandAck unexpected;
    unexpected.command_id = 88;
    unexpected.device = 60;
    unexpected.status = aethon::control::AckStatus::accepted;
    unexpected.time_ns = 250;
    auto miss = tracker.observe(unexpected);
    AETHON_REQUIRE(miss.kind == aethon::control::AckObservationKind::unexpected);

    aethon::control::AckExpectation timeout;
    timeout.command_id = 99;
    timeout.device = 61;
    timeout.created_time_ns = 100;
    timeout.deadline_ns = 150;
    AETHON_REQUIRE(tracker.expect(timeout));
    auto timed_out = tracker.timeouts(200);
    AETHON_REQUIRE(timed_out.size() == 1);
    AETHON_REQUIRE(timed_out.front().state == aethon::control::AckExpectationState::timed_out);
    AETHON_REQUIRE(aethon::control::render_ack_tracker_stats(tracker.stats()).find("duplicates") != std::string::npos);
    auto summaries = aethon::control::summarize_ack_devices(tracker.snapshots());
    AETHON_REQUIRE(summaries.size() == 2);
    AETHON_REQUIRE(aethon::control::render_ack_device_summaries(summaries).find("device: 60") != std::string::npos);
}
