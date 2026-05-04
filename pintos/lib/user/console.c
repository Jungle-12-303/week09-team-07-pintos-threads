#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscall-nr.h>

/* The standard vprintf() function,
   which is like printf() but uses a va_list. */
/* 표준 vprintf() 함수입니다.
   printf() 함수와 유사하지만 va_list를 사용합니다. */
int
vprintf (const char *format, va_list args) {
	return vhprintf (STDOUT_FILENO, format, args);
}

/* Like printf(), but writes output to the given HANDLE. */
/* printf()와 유사하지만, 지정된 핸들에 출력을 씁니다. */
int
hprintf (int handle, const char *format, ...) {
	va_list args;
	int retval;

	va_start (args, format);
	retval = vhprintf (handle, format, args);
	va_end (args);

	return retval;
}

/* Writes string S to the console, followed by a new-line
   character. */
/* 문자열 S를 콘솔에 출력하고 그 뒤에 줄 바꿈 문자를 추가합니다. */
int
puts (const char *s) {
	write (STDOUT_FILENO, s, strlen (s));
	putchar ('\n');

	return 0;
}

/* Writes C to the console. */
int
putchar (int c) {
	char c2 = c;
	write (STDOUT_FILENO, &c2, 1);
	return c;
}

/* Auxiliary data for vhprintf_helper(). */
/* vhprintf_helper() 함수에 필요한 보조 데이터입니다. */
struct vhprintf_aux {
	char buf[64];       /* Character buffer. */
	char *p;            /* Current position in buffer. */
	int char_cnt;       /* Total characters written so far. */
	int handle;         /* Output file handle. */
};

static void add_char (char, void *);
static void flush (struct vhprintf_aux *);

/* Formats the printf() format specification FORMAT with
   arguments given in ARGS and writes the output to the given
   HANDLE. */
/* printf() 형식 지정자 FORMAT을 ARGS에 지정된 인수로 포맷하고,
   지정된 HANDLE에 출력을 씁니다. */
int
vhprintf (int handle, const char *format, va_list args) {
	struct vhprintf_aux aux;
	aux.p = aux.buf;
	aux.char_cnt = 0;
	aux.handle = handle;
	__vprintf (format, args, add_char, &aux);
	flush (&aux);
	return aux.char_cnt;
}

/* Adds C to the buffer in AUX, flushing it if the buffer fills
   up. */
/* AUX 버퍼에 C를 추가하고, 버퍼가 가득 차면 버퍼를 비웁니다. */
static void
add_char (char c, void *aux_) {
	struct vhprintf_aux *aux = aux_;
	*aux->p++ = c;
	if (aux->p >= aux->buf + sizeof aux->buf)
		flush (aux);
	aux->char_cnt++;
}

/* Flushes the buffer in AUX. */
static void
flush (struct vhprintf_aux *aux) {
	if (aux->p > aux->buf)
		write (aux->handle, aux->buf, aux->p - aux->buf);
	aux->p = aux->buf;
}
