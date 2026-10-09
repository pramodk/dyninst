# Stackwalker rooted-path cache diagnostic

This manual diagnostic compares one Stackwalker client holding multiple host
or chrooted targets. It is intentionally not registered with the testsuite.

`walk16` matches the minimal reported client and avoids instrumentation in its
timed region. `walk_many` records elapsed time, walk/name/library success,
distinct reported library names, `/proc/self/status`,
`/proc/self/smaps_rollup`, and raw `malloc_info()` XML after attach, walking,
name lookup, library lookup, detach, and walker destruction.

Build against an installed Dyninst prefix:

```bash
DYNINST=/path/to/install CXX=/path/to/c++ ./build.sh
```

Run fresh clients for each operation so cache order does not mix the results:

```bash
./run.sh host 16 compat
./run.sh container 16 compat
./run.sh host 16 walk
./run.sh host 16 name
./run.sh host 16 liboffset
./run.sh host 16 both
./run.sh container 16 walk
./run.sh container 16 name
./run.sh container 16 liboffset
./run.sh container 16 both
./summarize.py results/*
```

The container case requires unprivileged user namespaces. Each process gets a
separate user and mount namespace and uses the same minimal chroot containing
the target, `libfoo.so`, the dynamic loader, and libc.
