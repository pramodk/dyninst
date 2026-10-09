# AArch64 results, 2026-10-09

These measurements used branch `pramodk/stat-aarch64-upstream` at
`a01928370284`. Commit `d28ffc7c30e197b25e11b5216f2b6396b7349a74` is an
ancestor of this revision. The machine was AArch64 Ubuntu 24.04 and the client
was built with GCC 15.3.0.

All runs attached one client to 16 targets and kept every walker attached until
the end. Every run found 160 frames (10 per target), named all 160 frames, and
completed without thread-list or stack-walk failures.

## Minimal reported client

`walk16` has no instrumentation in its timed region. Three fresh runs produced:

| target layout | time (s) | client VmRSS (MiB) | distinct library names |
| --- | ---: | ---: | ---: |
| host | 0.068, 0.073, 0.068 | 59.5, 58.9, 58.9 | 3 |
| separate namespace/chroot | 0.815, 0.810, 0.818 | 270.9, 270.7, 270.2 | 48 |

The container names were three PID-rooted paths per target: `/proc/PID/exe`,
`/proc/PID/root/app/libfoo.so`, and
`/proc/PID/root/lib/aarch64-linux-gnu/libc.so.6`. Host targets shared three
names.

This reproduces the reported direction and mechanism: PID-specific rooted paths
prevent pathname-keyed cache reuse across targets. On this machine the median
container run was about 12 times slower and used 4.6 times the client RSS.
It does not reproduce the report's absolute 48 seconds and 6.9 GiB, which came
from a different x86_64 build and machine.

## Instrumented operation isolation

`walk_many` used a fresh client for each row. The values below are after all 16
targets. `malloc_info` in-use is estimated as current arena bytes minus free
fast/rest bytes plus mmap bytes. It is allocator accounting, unlike VmRSS, and
does not include every resident mapping. The starting client was 18.4 MiB RSS
with approximately 0.8 MiB allocator in use.

| requested operations | host RSS (MiB) | container RSS (MiB) | host malloc in use (MiB) | container malloc in use (MiB) |
| --- | ---: | ---: | ---: | ---: |
| walk only | 57.6 | 157.3 | 7.4 | 60.1 |
| walk + `getName` | 59.0 | 177.3 | 8.6 | 78.6 |
| walk + `getLibOffset` | 57.8 | 252.4 | 7.4 | 112.9 |
| walk + both lookups | 59.1 | 269.8 | 8.6 | 131.4 |

The modes overlap internally and therefore are not additive. The data shows
that rooted-path duplication begins during attach/walk and that symbol/library
lookups amplify it, especially `getLibOffset`. RSS, anonymous RSS, private dirty
memory, and allocator in-use bytes all rose approximately linearly with the
number of container targets. Detaching and deleting the walkers did not return
the accumulated memory, consistent with process-global caches retaining it.

The instrumented elapsed times include repeated status, `smaps_rollup`, and
`malloc_info` collection and must not be compared with `walk16` timing.
