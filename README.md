File System in Userspace (FUSE) with GUI Integration
Developer Information
Developer: Shekinah B. Mulenga
Partner: Gloria Chomba
Overview
This project demonstrates the simulation of a simple file system in user space, implemented using C with a GTK3/4 graphical interface. It features core file system functionalities such as file management, block allocation, and Access Control List (ACL) permission handling.

The implementation offers both block and inode management and includes a journal to track operations for potential recovery. Though developed primarily for Unix-like environments, it can run on Windows with some limitations regarding permission handling.

Features
Block and Inode Management:
Direct and indexed block allocation techniques are used for efficient file storage.
File Operations:
Create, delete, rename, and modify files.
Access Control Lists (ACLs):
Fine-grained permission management for files.
Journaling:
Records operations to facilitate recovery.
Graphical User Interface (GUI):
Provides a user-friendly GTK-based interface for managing the file system.
Key Components
Superblock: Stores file system metadata (total size, block size, etc.).
Inodes: Represents files and directories.
Data Blocks: Store actual file content.
Journal: Logs operations for crash recovery.
ACLs: Set file access permissions for different users.
GUI: Provides graphical interaction for users via GTK3/4.
Allocation Techniques
Direct Allocation:
Each file’s inode has direct pointers to a fixed number of blocks.
Indexed Allocation:
Additional blocks are allocated using indirect addressing, expanding the file size beyond direct block capacity.
Limitations
Windows Compatibility:
The simulation runs on Windows with some limitations in permission restoration, as the operating system does not natively support Unix-style inodes. This means file permissions may not be fully restored on recovery.
Unix-Like Permissions:
While Unix-like permission operations are simulated, Windows systems do not support full inode functionality, which impacts permission handling and restoration.
Permissions
The file system uses a permission scheme based on Unix-style bit masks:

0400: Owner read
0200: Owner write
0100: Owner execute
0777: Full permissions (read, write, execute for all)
Requirements
CodeBlocks IDE or any other IDE capable of compiling C code.
GTK3/4 library must be installed for GUI functionality.
Installation and Usage
Clone the repository:

bash
Copy code
git clone https://github.com/yourusername/fs-userspace.git
Open the project:

Import the fsWithoutPermissions.c into CodeBlocks or your preferred IDE.
Install Dependencies: Ensure GTK3/4 is installed on your system:

Ubuntu:
bash
Copy code
sudo apt-get install libgtk-3-dev
Windows: Download and install the GTK library.
Compile the project:

Build and run the code from your IDE, ensuring GTK is properly linked.
Run the File System:

The GUI will allow you to create, delete, rename, and manage files with simulated permissions and journaling.
Example Commands
Create a file: Use the GUI to select "Create File" and input the desired file name.
Delete a file: Select the file from the file list in the GUI and click "Delete."
Change permissions: Adjust file permissions using the ACL interface.
Known Issues
Windows Permissions: While the system simulates permission handling, file permissions may not persist after recovery due to Windows' lack of native support for inodes and ACLs.
