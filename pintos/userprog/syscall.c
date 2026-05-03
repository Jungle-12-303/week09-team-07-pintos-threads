#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");
	thread_exit ();
}














































// /* The main system call interface */
// void
// syscall_handler (struct intr_frame *f) {

// 	switch (f->R.rax) {
// 		case SYS_WRITE: {
// 			/* SYS_WRITE arguments: fd, buffer, size. */
// 			/* 반환값은 f->R.rax에 저장 */
// 			int fd = (int) f->R.rdi;  /* fd */
// 			const char *buffer = (const char *) f->R.rsi;  /* user buffer address */
// 			unsigned size = (unsigned) f->R.rdx;

// 			validate_user_buffer (buffer, size); // 버퍼 검증 함수

// 			if (fd == 1) {
// 				putbuf (buffer, size);
// 				f->R.rax = size;
// 			} else {
// 				f->R.rax = -1;
// 			}
// 			break;
// 		}

// 		case SYS_EXIT:
// 			/* f->R.rdi가 exit status */
// 			exit_process ((int) f->R.rdi);
// 			break;

// 		case SYS_HALT:
// 			power_off(); // OS 종료
// 			break;

// 		default:
// 			/* 잘못된 syscall이면 프로세스 종료 */
// 			exit_process(-1);
// 			break;
// 	}
// }

// /* syscall arguments are passed in f->R.* registers.
//    Store the syscall return value in f->R.rax. */

// /* 입력값 검증 헬퍼 함수 */
// static void
// validate_user_ptr (const void *uaddr) {
// 	// 만약 이 user address가 현재 프로세스 주소 공간에서 유효하지 않을 경우 exit
// 	if (uaddr == NULL || !is_user_vaddr (uaddr) ||
// 	    pml4_get_page (thread_current ()->pml4, uaddr) == NULL) {
// 		exit_process (-1);
// 	}
// }

// static void
// validate_user_buffer (const void *buffer, size_t size) {

// 	// size == 0이면 접근할 메모리가 없으므로 바로 성공 처리
// 	if (size == 0) {
// 		return;
// 	}

// 	if (buffer == NULL) {
// 		exit_process (-1);
// 	}

// 	// 시작, 끝 주소를 정수로 계산하고 overflow를 확인
// 	uintptr_t start_addr = (uintptr_t) buffer;
// 	uintptr_t last_addr = start_addr + size - 1;

// 	if (last_addr < start_addr) {
// 		exit_process (-1);
// 	}

// 	// 바이트 단위 포인터 산술을 위해 uint8_t 포인터로 변환한다.
// 	const uint8_t *start = (const uint8_t *) start_addr;
// 	const uint8_t *last = (const uint8_t *) last_addr;
// 	const uint8_t *page_start = (const uint8_t *) pg_round_down (start);
// 	const uint8_t *page_end = (const uint8_t *) pg_round_down (last);

// 	// buffer가 걸친 모든 페이지가 매핑되어 있는지 확인
// 	for (const uint8_t *page = page_start; page <= page_end; page += PGSIZE) {
// 		validate_user_ptr (page);
// 	}

// 	// 실제 접근 시작, 끝 주소가 user 영역 안에 있는지 확인한다.
// 	validate_user_ptr (start);
// 	validate_user_ptr (last);
// }

// static void
// validate_user_string (const char *str) {
// 	/* TODO: 검증 로직 구현 필요 */
// 	/* 구현 후 open/create/exec에서 사용하면 됨 */
// }

// static void
// exit_process (int status) {
// 	printf ("%s: exit(%d)\n", thread_name (), status);
// 	/* TODO: thread 에 exit_status 저장 로직 구현 필요 */
// 	// 현재는 임시 구현이며, 향후 thread->exit_status 추가하고 0, -1 같은 프로세스 종료 코드를 넣는 등의 조치 필요
// 	thread_exit ();
// }
