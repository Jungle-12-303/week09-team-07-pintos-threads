#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
static bool user_addr_mapped (const void *uaddr);

struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	lock_init (&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

static bool
user_addr_mapped (const void *uaddr) {
	struct thread *cur = thread_current ();

	return uaddr != NULL
		&& is_user_vaddr (uaddr)
		&& cur->pml4 != NULL
		&& pml4_get_page (cur->pml4, uaddr) != NULL;
}

void
user_check_ptr (const void *uaddr) {
	if (!user_addr_mapped (uaddr))
		process_exit_with_status (-1);
}

void
user_check_read (const void *uaddr, size_t size) {
	uint64_t start;
	uint64_t last;
	uint64_t page;

	if (size == 0)
		return;
	if (uaddr == NULL)
		process_exit_with_status (-1);

	start = (uint64_t) uaddr;
	last = start + size - 1;
	if (last < start)
		process_exit_with_status (-1);

	for (page = (uint64_t) pg_round_down ((void *) start);
			page <= (uint64_t) pg_round_down ((void *) last);
			page += PGSIZE)
		user_check_ptr ((const void *) page);
}

void
user_check_write (void *uaddr, size_t size) {
	user_check_read (uaddr, size);
}

void
user_check_string (const char *uaddr) {
	const char *p;

	for (p = uaddr; ; p++) {
		user_check_ptr (p);
		if (*p == '\0')
			return;
	}
}

char *
user_strdup (const char *uaddr) {
	char *copy;
	size_t i;

	copy = palloc_get_page (0);
	if (copy == NULL)
		process_exit_with_status (-1);

	for (i = 0; i < PGSIZE; i++) {
		user_check_ptr (uaddr + i);
		copy[i] = uaddr[i];
		if (copy[i] == '\0')
			return copy;
	}

	palloc_free_page (copy);
	process_exit_with_status (-1);
}

void
syscall_handler (struct intr_frame *f) {
	// x86-64 호출 규약에서 함수 반환값은 RAX 레지스터에 두어야 합니다. 반환값이 있는 시스템 콜은 struct intr_frame의 rax 멤버를 수정해 이를 구현할 수 있습니다.

	switch (f->R.rax)
	{
	/* Projects 2 and later. */
	case SYS_HALT:                   /* Halt the operating system. */
		power_off(); // OS 종료
		break;
	case SYS_EXIT:                   /* Terminate this process. */
		process_exit_with_status((int) f->R.rdi);
		break;
	case SYS_FORK:// TODO: B                   /* Clone current process. */
		f->R.rax = -1;
		break;
	case SYS_EXEC:// TODO: B                   /* Switch current process. */
		f->R.rax = -1;
		break;
	case SYS_WAIT:// TODO: B                   /* Wait for a child process to die. */
		f->R.rax = -1;
		break;
	case SYS_CREATE:// TODO: A                 /* Create a file. */
		// 값 들어 오는 것 확인
		// rdi로 데이터 들어옴
		filesys_create((char *)f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:// TODO: A                 /* Delete a file. */
		f->R.rax = -1;
		break;
	case SYS_OPEN:// TODO: A                   /* Open a file. */
		f->R.rax = -1;
		break;
	case SYS_FILESIZE:// TODO: A               /* Obtain a file's size. */
		f->R.rax = -1;
		break;
	case SYS_READ:// TODO: A                   /* Read from a file. */
		f->R.rax = -1;
		break;
	case SYS_WRITE: { // TODO: A               /* Write to a file. */
		int fd = (int) f->R.rdi;
		const char *buffer = (const char *) f->R.rsi;  /* user buffer address */
		unsigned size = (unsigned) f->R.rdx;

		// 사용자 버퍼 검증 함수 작성 필요

		if (fd == 1) {
			putbuf (buffer, size);
			f->R.rax = size;
		} else {
			f->R.rax = -1;
		}

		break;
	}
	case SYS_SEEK:// TODO: A                   /* Change position in a file. */
		f->R.rax = -1;
		break;
	case SYS_TELL:// TODO: A                   /* Report current position in a file. */
		f->R.rax = -1;
		break;
	case SYS_CLOSE:// TODO: A                  /* Close a file. */
		f->R.rax = -1;
		break;
	default:
		process_exit_with_status (-1); // 프로세스를 비정상 종료
		break;
	}
}
