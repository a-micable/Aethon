# Submission: Reentrant EventDispatcher Mutation Causes Heap Use-After-Free

## Bug Name

Reentrant `EventDispatcher::publish` handler mutation invalidates active handler iterators

## Summary

`EventDispatcher::publish` iterates directly over `handlers_`, a `std::vector<std::function<void(const RuntimeEvent&)>>`. Event handlers are arbitrary callbacks and can reenter the dispatcher by calling `subscribe()` or `clear()` on the same dispatcher while `publish()` is still iterating.

If a callback calls `subscribe()` enough times to grow `handlers_`, the vector reallocates and frees the storage currently being used by the active range-for loop in `publish()`. When `publish()` continues iteration, it reads from freed vector storage, producing a deterministic ASan heap-use-after-free.

This is not an OOM or large-allocation bug. It is a callback reentrancy/lifetime bug in runtime event dispatch.

## Affected Code

`src/runtime/event_dispatcher.cpp`

```cpp
void EventDispatcher::publish(const RuntimeEvent& event) const {
    for (const auto& handler : handlers_) {
        handler(event);
    }
}
```

The loop keeps iterators/references into `handlers_` while user-controlled callbacks are executing. Those callbacks can mutate the same vector.

## C++ PoC

PoC source:

`pocs/event_dispatcher_reentrant_uaf_poc.cpp`

Build against an ASan/UBSan build of the project:

```sh
c++ -std=c++20 -fsanitize=address,undefined -g -O1 \
  -Iinclude \
  pocs/event_dispatcher_reentrant_uaf_poc.cpp \
  build-gcc-asan/libaethon.a \
  -o /tmp/event_dispatcher_reentrant_uaf_poc
```

Run:

```sh
ASAN_OPTIONS=detect_leaks=0 /tmp/event_dispatcher_reentrant_uaf_poc
```

Expected result on the vulnerable build:

```text
ERROR: AddressSanitizer: heap-use-after-free
    #... std::function<void (aethon::runtime::RuntimeEvent const&)>::operator()
    #... aethon::runtime::EventDispatcher::publish(...)
    #... main pocs/event_dispatcher_reentrant_uaf_poc.cpp
freed by thread T0 here:
    #... std::vector<RuntimeEventHandler>::_M_realloc_insert(...)
```

## Root Cause

The dispatcher allows arbitrary callback code to execute while iterating over the mutable backing vector. Reentrant `subscribe()` reallocates `handlers_`, invalidating the iterator and reference used by the active range-for loop.

## Impact

Any runtime component exposing event subscriptions through `EventDispatcher` can be crashed by a subscriber that mutates subscriptions during event publication. The affected call paths include runtime collector, ingest queue, and replay driver event publishing. In a larger embedding, this can be triggered by plugin-style or operator-provided event handlers.

## Patch

Apply:

`patches/event_dispatcher_reentrant_uaf.patch`

The fix snapshots the handler list before invoking callbacks:

```cpp
auto handlers = handlers_;
for (const auto& handler : handlers) {
    handler(event);
}
```

This prevents reentrant mutation of the dispatcher’s backing vector from invalidating the active iteration.
