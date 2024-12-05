#!/bin/bash

# Check if process count is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <number_of_processes>"
    exit 1
fi

# Number of processes
num_processes=$1

# File paths (modify these if needed)
hosts_file="./custom_tests/hosts/fifo-hosts.txt"
config_file="./custom_tests/configs/fifo-config.config"
output_dir="./custom_tests/output"

# Ensure output directory exists
mkdir -p "$output_dir"

# Array to store PIDs of background processes
pids=()

# Trap SIGINT (Ctrl+C) to clean up child processes
trap 'echo "Terminating processes..."; kill ${pids[@]} 2>/dev/null; exit' SIGINT

# Start the remaining processes in the background
for ((i = 1; i <= num_processes; i++)); do
    echo "Starting process $i in background..."
    ./run.sh --id "$i" --hosts "$hosts_file" --output "$output_dir/$i.output" "$config_file" &
    pids+=($!)
done

# Wait for the first process (foreground) to finish
wait ${pids[0]}

echo "Waiting for background processes. Press Ctrl+C to terminate."
wait
