# Distributed Algorithms Project

A practical implementation of fundamental distributed system building blocks including Perfect Links, FIFO Broadcast, and Lattice Agreement.

## Overview

This project implements three key abstractions for reliable message delivery in decentralized systems:

1. **Perfect Links** - Reliable point-to-point communication
2. **FIFO Broadcast** - First-in-first-out ordered reliable broadcast
3. **Lattice Agreement** - Multi-shot consensus for comparable decisions

## System Model

- **Processes**: Static system of n processes (Ψ = {P₁, ..., Pₙ})
- **Failures**: Up to f processes can crash (n = 2f + 1)
- **Asynchrony**: Processes and network are asynchronous with finite but arbitrary delays
- **Communication**: Authenticated point-to-point network with possible message loss, delay, and reordering

## Abstractions

### 1. Perfect Links

Provides reliable point-to-point message delivery between processes.

**Events:**
- `⟨pl, Send | q, m⟩`: Send message m to process q
- `⟨pl, Deliver | p, m⟩`: Deliver message m from process p

**Properties:**
- **PL1 (Reliable delivery)**: If correct process p sends message m to correct process q, then q eventually delivers m
- **PL2 (No duplication)**: No message is delivered more than once
- **PL3 (No creation)**: Only previously sent messages are delivered

### 2. FIFO Broadcast

Extends uniform reliable broadcast with FIFO ordering guarantees.

**Events:**
- `⟨frb, Send | m⟩`: Broadcast message m to all processes
- `⟨frb, Deliver | p, m⟩`: Deliver message m broadcast by process p

**Properties:**
- **FRB1 (Validity)**: If correct process broadcasts m, then it eventually delivers m
- **FRB2 (No duplication)**: No message is delivered more than once
- **FRB3 (No creation)**: Only previously broadcast messages are delivered
- **FRB4 (Uniform agreement)**: If any process delivers m, then every correct process eventually delivers m
- **FRB5 (FIFO delivery)**: Messages from same sender are delivered in send order

### 3. Lattice Agreement

Multi-shot consensus algorithm allowing processes to agree on comparable decisions despite failures.

**Events:**
- `⟨la, Propose | Iᵢ⟩`: Process proposes set Iᵢ ⊆ V
- `⟨la, Decide | Oᵢ⟩`: Process decides set Oᵢ ⊆ V

**Properties:**
- **LA1 (Validity)**: Decided set includes proposal and only proposed values: Iᵢ ⊆ Oᵢ ⊆ ⋃ⱼ Iⱼ
- **LA2 (Consistency)**: All decisions are comparable: Oᵢ ⊆ Oⱼ or Oᵢ ⊃ Oⱼ
- **LA3 (Termination)**: Every correct process eventually decides

## Implementation Requirements

### Language Support
- **C/C++**: C11/C++17 with CMake build system
- **Java**: Java 11 with Maven build system
- No third-party libraries allowed (standard libraries only)

### Hardware Constraints
- 2 CPU cores, 4 GiB RAM
- Maximum 8 threads per process
- Output files ≤ 64 MiB
- Console output trimmed at 1 MiB
- Up to 128 processes, MAX_INT messages

### Communication
- UDP packets only (no additional network features)
- Single UDP socket per process for receiving
- Messages may be dropped, delayed, or reordered
- Maximum 8 messages per UDP packet
- Messages up to 64 KiB fit in single packet

## Project Structure

```
.
├── bin/
├── build.sh           # Compile project
├── run.sh             # Run project
├── cleanup.sh         # Clean build artifacts
├── src/               # Your implementation
└── tools/
    ├── stress.py      # Stress testing tool
    └── tc.py          # Network condition simulation
```

## Usage

### Command Line Interface

```bash
./run.sh --id ID --hosts HOSTS --output OUTPUT CONFIG
```

**Parameters:**
- `ID`: Unique process identifier (1 to n)
- `HOSTS`: File containing process information (id, host, port per line)
- `OUTPUT`: Output log file path
- `CONFIG`: Configuration file for the specific milestone

### Example HOSTS file
```
1 localhost 11001
2 localhost 11002
3 localhost 11003
```

## Milestones

### Milestone 1: Perfect Links

**Config format:**
```
m i
```
- `m`: Number of messages each sender sends
- `i`: Index of receiver process

**Output format:**
- `b seq_nr`: Broadcast message with sequence number
- `d sender seq_nr`: Deliver message from sender

### Milestone 2: FIFO Broadcast

**Config format:**
```
m
```
- `m`: Number of messages each process broadcasts

**Output format:**
- `b seq_nr`: Broadcast message
- `d sender seq_nr`: Deliver message from sender

### Milestone 3: Lattice Agreement

**Config format:**
```
p vs ds
proposal_1
proposal_2
...
```
- `p`: Number of proposals per process
- `vs`: Maximum elements per proposal
- `ds`: Maximum distinct elements across all proposals

**Output format:**
- Each line contains decided integers separated by spaces

## Testing

### Stress Testing
```bash
# Perfect Links
./stress.py perfect -r ./run.sh -l logs -p 5 -m 100

# FIFO Broadcast  
./stress.py fifo -r ./run.sh -l logs -p 5 -m 100

# Lattice Agreement
./stress.py agreement -r ./run.sh -l logs -p 5 -n 10 -v 3 -d 5
```

### Network Simulation
```bash
# Apply network conditions (delay, loss, reordering)
python3 tools/tc.py

# Run tests with network interference
./stress.py [test_type] [options]
```

## Signal Handling

- **SIGTERM/SIGINT**: Process must stop immediately (except logging)
- **SIGSTOP/SIGCONT**: Process execution pause/resume
- Signal handling is provided in the template

## Logging Requirements

- One output file per process
- Each event on separate line with Unix line breaks (`\n`)
- No duplicate events or incomplete lines
- Crashed processes must log events before crash
- Format must be exact - malformed output files are discarded

## Build and Run Example

```bash
# Build
./build.sh

# Terminal 1
./run.sh --id 1 --hosts hosts --output output/1.output config

# Terminal 2  
./run.sh --id 2 --hosts hosts --output output/2.output config

# Terminal 3
./run.sh --id 3 --hosts hosts --output output/3.output config

# Stop with Ctrl-C when done
```

## Key Implementation Notes

- Implement abstractions incrementally (Perfect Links → FIFO Broadcast → Lattice Agreement)
- Handle asynchronous message delivery and process failures
- Optimize for performance while maintaining correctness
- Test thoroughly with various network conditions and failure scenarios
- Memory management crucial for large-scale tests
- Single-shot lattice agreement algorithm provided as reference for multi-shot implementation
