# GSoC 2024: Emulating Missing Linux Syscalls: Tackling “The L2N Problem”

NetBSD with the it's in-kernel ABI layer `COMPAT LINUX` enables support for running Linux binaries by means of emulation on NetBSD systems. This layer does the mapping of Linux system calls to their NetBSD counterparts by identifying Linux binaries and then trapping the calls to Linux system calls. As newer features are added to the Linux kernel, newer system calls are also added when they are deemed essential. This project attempts to identify some of the such critical newer system calls that currently have no emulation support by means of the Linux Test Project and add support for them in the emulation layer by following a mix of some of the strategies proposed in the plan. This extends the NetBSD kernel emulation completeness, trying to tackle the **L**inux**2N**etBSD binary execution problem, one system call at a time.


## Implementation Details

#### `semtimedop` 
`semtimedop` is implemented in the kernel as a native system call. Difference between `semop` and `semtimedop` is, the latter accepts a `timeout` argument which specifies maximum time a thread can sleep while waiting to perform semaphore operations. This was identified as an important system call for NetBSD system hence is implemented natively in the NetBSD kernel. The major change from `semop` was how threads are put to sleep while waiting. Primarily `cv_wait_sig` condition variable in `semop` needed to be replaced with `cv_timedwait_sig` to honour the `timeout` argument. To reduce code duplication a common internal function `do_semop` was introduced for both `semop` and `semtimedop` system calls to call.

[Merge link](https://github.com/NetBSD/src/commit/ae03c1998c29b7b9ee911838ae495a26c5080cc4) 
#### `copy_file_range`
`copy_file_range` in linux allows copy offloading. But this expects file systems support to effectively utilise copy offloading, without which it calls into a fallback mechanism. Considering this, a similar fallback mechanism has been implemented in NetBSD. This used `uvm` objects to perform reads and writes on given ranges and used a fixed size buffer to implement a looped copying mechanism. File offsets were then updated accordingly.

[Merge link](https://github.com/NetBSD/src/commit/a3740e150a4bea0d03e70539dc6fe36956b72c38) 
#### `renameat2`
`renameat2` introduced a new `flags` argument to the `renameat` system call. Initial plan was to support two of the three new flags `RENAME_EXCHANGE` and `RENAME_NOREPLACE`. [Initial attempts](https://github.com/coderaccords/gsoc24-netbsd-linux-emulation/commit/c5e5c235e3cf8ccfadef6bd25d1e4d30f0fea599) were made to do the same with using `VOP_RENAME`internally. But after [community discussions](https://mail-index.netbsd.org/tech-kern/2024/09/14/msg029727.html) it became apparent that this cannot exactly be done without breaking atomicity of lookups. So `renameat2` emulation currently performs sanity checks for flag combinations and calls into the existing `do_sys_renameat` 

[Merge link](https://github.com/NetBSD/src/commit/a3740e150a4bea0d03e70539dc6fe36956b72c38) 
#### `faccessat2`, `fchmodat4`
`faccessat2` was introduced in linux to conform to POSIX standard in kernel without having to rely on libc emulation of the same. POSIX compliant `AT_EACCESS` and `AT_SYMLINK_FOLLOW` were added as flags to the system call. NetBSD kernel already supported these flags, so the implementation ideally only needed translation of arguments for NetBSD and then a call to `do_sys_accessat` with translated arguments. But this warranted [changes](https://github.com/NetBSD/src/commit/b79b3edc5f00c2bdb48f07c66224e63db5538ec0) to `fd_nameiat` function in `vfs_syscalls.c` to handle absolute path names in accordance with POSIX.  
`fchmodat4`had same changes proposed but it never got merged with linux kernel, so nothing was done for it.

[Merge link](https://github.com/NetBSD/src/commit/bc9a9bdb0c4a566e89fca9e351af8f61e55e7823)
#### `clone3`
`clone3` proposed many improvements to the pre-existing `clone` system call's API. It isn't possible to support all of them (these are independent projects themselves). So, instead `clone3` arguments were parsed and depending on parsed arguments a redirection was made depending on whether the requested features can be enabled with current `clone`call. The arguments were appropriately translated to the format expected by `clone` and the calls were redirected.

[Merge link](https://github.com/NetBSD/src/commit/a3740e150a4bea0d03e70539dc6fe36956b72c38)
#### `syncfs`, `sync_file_range`
`sync_file_range` allows to sync open file's range on disk. A similar functionality is provided in NetBSD by `fsync_range`. The difference is in `fsync_range` the ranges/offsets weren't rounded up to page boundaries. `sync_file_range` expected this behaviour so this was implemented in compat layer before calling `fsync_range`

[Merge link](https://github.com/NetBSD/src/commit/a3740e150a4bea0d03e70539dc6fe36956b72c38)

`sync_fs` allows syncing a file system based on an open `fd` provided as argument. No high-level NetBSD equivalent function existed to perform the same. Instead `VFS_SYNC` allowed to perform the same. Necessary data structures like `vnode`, `mount`, etc. were derived from the `fd` and corresponding call to `VFS_SYNC` was performed for to commit buffer cache to disk. 

[Merge link](https://github.com/NetBSD/src/commit/a3740e150a4bea0d03e70539dc6fe36956b72c38#diff-2a1cf5dc3356dfc32b1044bf786b3491703509cc4a9d3b6bc61450bffec7c589)
#### `getcpu`
The `lwp` structure that was used to identify a calling thread already had a `l_cpu` structure to store information related to the cpu and numa nodes. This information was retrieved and sent back to the caller.

[Merge link](https://github.com/NetBSD/src/commit/bc9a9bdb0c4a566e89fca9e351af8f61e55e7823)

## Setup
NetBSD has an excellent guide and tools for cross-compiling the whole development toolchain and distribution. A cross-build setup across an x86 emulator running NetBSD and ARM mac was setup as an NetBSD test machine.
### system setup
A UTM based VM was used to run NetBSD kernel. Most of the changes involved compat layer were applied by using `COMPAT_LINUX` module to a running kernel. When kernel changes were performed, new kernel binary was pasted into `/` directory of VM and a reboot was performed to apply and verify those changes.
A port on VM was exposed for establishing SSH connection with the running VM image. For enabling easy file transfer an `sshfs` drive was also mounted on the host system.
To call a newly inserted system call from user space, an entry needs to be inserted into `libc` and libc needs to be rebuilt. The rebuilt `libc.so` and relevant files were transferred to replace the default `libc` used by applications.

## Testing Framework	
Two different testing frameworks were used depending on the type of system call being tested.
### 1. Linux System Calls
To test linux system calls, Linux Test Project binaries were typically used. All the binaries were statically compiled to remove the need for presence of linux compiled dynamic libraries in the NetBSD test VM.
### 2. Native System Calls
To test native system calls as well as kernel enhancements, NetBSD's Automated Testing Framework was used. Newer tests were added to check functionality of changes made into kernel as well as system calls added to the kernel. Some necessary ones also got merged ([1](https://github.com/NetBSD/src/commit/0c467abdbb0737153c4abfc97f20445a27cd6f6e), [2](https://github.com/NetBSD/src/commit/a46b73af2314aab2cdc4db84560bf9003cd5fc1b)) into the NetBSD tests. 

## Closing remarks
NetBSD has a thriving and welcoming community that is always there for help when you need to solicit ideas about when you are stuck. In particular I would like to thank my mentor Christos Zoulas for always helping me to figure out things and at times even hand holding me. Thanks a lot, Christos!

The system calls extended in this project particularly involved file systems and inter process communication primitives. Working on them at kernel-level provided me with an invaluable appreciation for their functionality and all the intricacies involved. 

Working on an unfamiliar software, especially something as massive and impactful as kernel comes with it's fair shares of challenges and Aha! moments. My dive into NetBSD kernel was no different. From writing system calls to writing tests for those system calls to finally adding [manual page entries](https://github.com/NetBSD/src/commit/4d27234e8ccea503b22ce787da880843cc749a13) for them, I got to learn lots of new things. I am extremely grateful for all the exposure this project has provided me. I would like to thank the NetBSD foundation for accepting my proposal as well as GSoC for providing me with this opportunity!
