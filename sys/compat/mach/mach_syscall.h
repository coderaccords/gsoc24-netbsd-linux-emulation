/* $NetBSD: mach_syscall.h,v 1.13 2003/12/30 00:15:46 manu Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.5 2003/01/18 08:18:50 thorpej Exp 
 */

/* syscall: "reply_port" ret: "mach_port_name_t" args: */
#define	MACH_SYS_reply_port	26

/* syscall: "thread_self_trap" ret: "mach_port_name_t" args: */
#define	MACH_SYS_thread_self_trap	27

/* syscall: "task_self_trap" ret: "mach_port_name_t" args: */
#define	MACH_SYS_task_self_trap	28

/* syscall: "host_self_trap" ret: "mach_port_name_t" args: */
#define	MACH_SYS_host_self_trap	29

/* syscall: "msg_trap" ret: "mach_msg_return_t" args: "mach_msg_header_t *" "mach_msg_option_t" "mach_msg_size_t" "mach_msg_size_t" "mach_port_name_t" "mach_msg_timeout_t" "mach_port_name_t" */
#define	MACH_SYS_msg_trap	31

/* syscall: "msg_overwrite_trap" ret: "mach_kern_return_t" args: "mach_msg_header_t *" "mach_msg_option_t" "mach_msg_size_t" "mach_msg_size_t" "mach_port_name_t" "mach_msg_timeout_t" "mach_port_name_t" "mach_msg_header_t *" "mach_msg_size_t" */
#define	MACH_SYS_msg_overwrite_trap	32

/* syscall: "semaphore_signal_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" */
#define	MACH_SYS_semaphore_signal_trap	33

/* syscall: "semaphore_signal_all_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" */
#define	MACH_SYS_semaphore_signal_all_trap	34

/* syscall: "semaphore_signal_thread_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_port_name_t" */
#define	MACH_SYS_semaphore_signal_thread_trap	35

/* syscall: "semaphore_wait_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" */
#define	MACH_SYS_semaphore_wait_trap	36

/* syscall: "semaphore_wait_signal_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_port_name_t" */
#define	MACH_SYS_semaphore_wait_signal_trap	37

/* syscall: "semaphore_timedwait_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" "unsigned int" "mach_clock_res_t" */
#define	MACH_SYS_semaphore_timedwait_trap	38

/* syscall: "semaphore_timedwait_signal_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_port_name_t" "unsigned int" "mach_clock_res_t" */
#define	MACH_SYS_semaphore_timedwait_signal_trap	39

/* syscall: "init_process" ret: "mach_kern_return_t" args: */
#define	MACH_SYS_init_process	41

/* syscall: "map_fd" ret: "mach_kern_return_t" args: "int" "mach_vm_offset_t" "mach_vm_offset_t *" "mach_boolean_t" "mach_vm_size_t" */
#define	MACH_SYS_map_fd	43

/* syscall: "task_for_pid" ret: "mach_kern_return_t" args: "mach_port_t" "int" "mach_port_t *" */
#define	MACH_SYS_task_for_pid	45

/* syscall: "pid_for_task" ret: "mach_kern_return_t" args: "mach_port_t" "int *" */
#define	MACH_SYS_pid_for_task	46

/* syscall: "macx_swapon" ret: "mach_kern_return_t" args: "char *" "int" "int" "int" */
#define	MACH_SYS_macx_swapon	48

/* syscall: "macx_swapoff" ret: "mach_kern_return_t" args: "char *" "int" */
#define	MACH_SYS_macx_swapoff	49

/* syscall: "macx_triggers" ret: "mach_kern_return_t" args: "int" "int" "int" "mach_port_t" */
#define	MACH_SYS_macx_triggers	51

/* syscall: "swtch_pri" ret: "mach_kern_return_t" args: "int" */
#define	MACH_SYS_swtch_pri	59

/* syscall: "swtch" ret: "mach_kern_return_t" args: */
#define	MACH_SYS_swtch	60

/* syscall: "syscall_thread_switch" ret: "mach_kern_return_t" args: "mach_port_name_t" "int" "mach_msg_timeout_t" */
#define	MACH_SYS_syscall_thread_switch	61

/* syscall: "clock_sleep_trap" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_sleep_type_t" "int" "int" "mach_timespec_t *" */
#define	MACH_SYS_clock_sleep_trap	62

/* syscall: "timebase_info" ret: "mach_kern_return_t" args: "mach_timebase_info_t" */
#define	MACH_SYS_timebase_info	89

/* syscall: "wait_until" ret: "mach_kern_return_t" args: "u_int64_t" */
#define	MACH_SYS_wait_until	90

/* syscall: "timer_create" ret: "mach_port_name_t" args: */
#define	MACH_SYS_timer_create	91

/* syscall: "timer_destroy" ret: "mach_kern_return_t" args: "mach_port_name_t" */
#define	MACH_SYS_timer_destroy	92

/* syscall: "timer_arm" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_absolute_time_t" */
#define	MACH_SYS_timer_arm	93

/* syscall: "timer_cancel" ret: "mach_kern_return_t" args: "mach_port_name_t" "mach_absolute_time_t *" */
#define	MACH_SYS_timer_cancel	94

/* syscall: "get_time_base_info" ret: "mach_kern_return_t" args: */
#define	MACH_SYS_get_time_base_info	95

#define	MACH_SYS_MAXSYSCALL	128
#define	MACH_SYS_NSYSENT	128
