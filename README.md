# PandOS Operating System

## Overview
PandOS is an educational operating system implementation for the UMPS3 architecture. This project implements a microkernel operating system with process management, synchronization, I/O handling, and exception management capabilities.

## Project Structure
The project is organized as follows:

## Core Components

### Process Management
- **PCB:** Process Control Block implementation with functions for allocation, deallocation, and management of process queues and trees
- **Scheduler:** Two-level priority scheduler with high and low priority queues and round-robin time slicing

### Synchronization
- **ASL:** Active Semaphore List implementation for process synchronization
- **Semaphores:** Integer-based semaphore implementation with P (wait) and V (signal) operations

### Exception Handling
- **System Calls:** Eight system calls for process and resource management
- **Exception Dispatcher:** Handles various exceptions and routes them to appropriate handlers
- **Pass Up or Die:** Mechanism for handling unrecoverable exceptions

### Interrupt Handling
- **Device Interrupts:** Handles interrupts from various devices including disk, terminal, network, and printers
- **Timer Interrupts:** Manages processor local timer and interval timer events

## System Calls
1. **CREATEPROCESS (SYS1):** Create a new process
2. **TERMINATEPROCESS (SYS2):** Terminate a process and all its descendants
3. **PASSEREN (SYS3):** P operation on semaphore (blocking wait)
4. **VERHOGEN (SYS4):** V operation on semaphore (signal)
5. **WAITIO (SYS5):** Wait for I/O operation completion
6. **GETCPUTIME (SYS6):** Get CPU time used by current process
7. **WAITCLOCK (SYS7):** Wait for clock tick
8. **GETSUPPORTPTR (SYS8):** Get support structure pointer

## Module Details

### Initial
The initialization module sets up the system data structures, Pass Up Vector for exceptions, and creates the first process running the test function.

### PCB
Implements a Process Control Block data structure and operations, including process queue management and parent-child relationships.

### ASL
Active Semaphore List manages semaphores and blocked processes, implementing operations to insert, remove, and search for processes blocked on semaphores.

### Scheduler
Implements a multilevel feedback queue scheduling algorithm with high and low priority queues, deadlock detection, and process loading.

### Exceptions
Handles all processor exceptions including system calls, TLB management, and program traps, implementing the "Pass Up or Die" philosophy.

### Interrupts
Handles all hardware interrupts including device I/O, processor local timer (PLT), and interval timer interrupts.

## Time Management
- **CPU Accounting:** Tracks CPU time used by each process
- **Time Slicing:** Implements round-robin scheduling with fixed time quantum
- **Interval Timer:** Provides system clock ticks every 100ms

## Contributors
- Aryah Rao
- Anish Reddy

## Building and Running
This project is designed to run on the UMPS3 emulator, an educational MIPS processor emulator.

1. Install UMPS3
2. Configure the machine with appropriate settings
3. Build the kernel using the provided Makefile
4. Load the kernel and start the emulation

## License
This project is an educational implementation and is provided for learning purposes.