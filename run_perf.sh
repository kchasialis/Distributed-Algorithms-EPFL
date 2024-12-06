#!/bin/bash

# Execute processes
./simulate.sh 9

sleep 1

# Profiling with perf
perf record -F 99 -p $(pgrep -d, da_proc) -g -- sleep 30 &
prof_pid=$!

wait $prof_pid
perf report > perf_results.txt