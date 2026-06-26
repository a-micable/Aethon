# Fuzzing

Aethon exposes independent libFuzzer harnesses for packet frames, optional protocol sections, archives, replay, live streams, configuration files, transform layers, and state-machine components.

The `fuzz/corpus` tree contains deterministic seeds generated from the production wire formats: valid ATHN packet frames, AETHARC1 archives, noisy stream captures, configuration files, protocol section blobs, transform inputs, and state-machine event bytes.

ClusterFuzzLite builds these targets via `.clusterfuzzlite/build.sh` and copies the dictionary plus corpus tree into `$OUT`.
