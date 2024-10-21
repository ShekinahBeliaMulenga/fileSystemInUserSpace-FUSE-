# File System in Userspace (FUSE) with GUI Integration

## Developer Information
- **Developer:** Shekinah B. Mulenga
- **Partner:** Gloria Chomba

## Overview

This project demonstrates the simulation of a simple **file system** in user space, implemented using **C** with a **GTK3/4** graphical interface. It features core file system functionalities such as file management, block allocation, and Access Control List (ACL) permission handling.

The implementation offers both **block and inode management** and includes a **journal** to track operations for potential recovery. Though developed primarily for Unix-like environments, it can run on **Windows** with some limitations regarding permission handling.

## Features
- **Block and Inode Management**: Direct and indexed block allocation techniques are used for efficient file storage.
- **File Operations**: Create, delete, rename, and modify files.
- **Access Control Lists (ACLs)**: Fine-grained permission management for files.
- **Journaling**: Records operations to facilitate recovery.
- **Graphical User Interface (GUI)**: Provides a user-friendly GTK-based interface for managing the file system.

## Key Components
1. **Superblock**: Stores file system metadata (total size, block size, etc.).
2. **Inodes**: Represents files and directories.
3. **Data Blocks**: Store actual file content.
4. **Journal**: Logs operations for crash recovery.
5. **ACLs**: Set file access permissions for different users.
6. **GUI**: Provides graphical interaction for users via **GTK3/4**.

## Allocation Techniques
1. **Direct Allocation**: Each file’s inode has direct pointers to a fixed number of blocks.
   
2. **Indexed Allocation**: Additional blocks are allocated using indirect addressing, expanding the file size beyond direct block capacity.

## Limitations
- **Windows Compatibility**: 
   The simulation runs on Windows with some limitations in permission restoration, as the operating system does not natively support Unix-style inodes. This means **file permissions** may not be fully restored on recovery.
   
- **Unix-Like Permissions**: 
   While Unix-like permission operations are simulated, Windows systems do not support full inode functionality, which impacts permission handling and restoration.

## Permissions
The file system uses a permission scheme based on Unix-style bit masks:
- `0400`: Owner read
- `0200`: Owner write
- `0100`: Owner execute
- `0777`: Full permissions (read, write, execute for all)

## Requirements
- **CodeBlocks IDE** or any other IDE capable of compiling **C** code.
- **GTK3/4** library must be installed for GUI functionality.

