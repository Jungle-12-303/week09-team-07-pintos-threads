#ifndef TESTS_LIB_H
#define TESTS_LIB_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <syscall.h>

extern const char *test_name;
extern bool quiet;

void msg (const char *, ...) PRINTF_FORMAT (1, 2);
void fail (const char *, ...) PRINTF_FORMAT (1, 2) NO_RETURN;

/* Takes an expression to test for SUCCESS and a message, which
   may include printf-style arguments.  Logs the message, then
   tests the expression.  If it is zero, indicating failure,
   emits the message as a failure.

   Somewhat tricky to use:

     - SUCCESS must not have side effects that affect the
       message, because that will cause the original message and
       the failure message to differ.

     - The message must not have side effects of its own, because
       it will be printed twice on failure, or zero times on
       success if quiet is set. */
/* 성공 여부를 테스트할 표현식과 메시지를 인수로 받습니다.
   메시지에는 printf 스타일의 인수가 포함될 수 있습니다. 메시지를 로그에 출력한 다음,
    표현식을 테스트합니다. 결과가 0이면(실패)
    실패 메시지를 출력합니다.

    사용법이 다소 까다롭습니다.

    - 성공 표현식은 메시지에 영향을 미치는 부작용이 없어야 합니다.
    그렇지 않으면 원래 메시지와
    실패 메시지가 달라지게 됩니다.

    - 메시지 자체에도 부작용이 없어야 합니다.
    그렇지 않으면 실패 시 두 번 출력되고,
    quiet가 설정된 경우 성공 시에는 출력되지 않습니다. */
#define CHECK(SUCCESS, ...)                     \
        do                                      \
          {                                     \
            msg (__VA_ARGS__);                  \
            if (!(SUCCESS))                     \
              fail (__VA_ARGS__);               \
          }                                     \
        while (0)

void shuffle (void *, size_t cnt, size_t size);

void exec_children (const char *child_name, pid_t pids[], size_t child_cnt);
void wait_children (pid_t pids[], size_t child_cnt);

void check_file_handle (int fd, const char *file_name,
                        const void *buf_, size_t filesize);
void check_file (const char *file_name, const void *buf, size_t filesize);

void compare_bytes (const void *read_data, const void *expected_data,
                    size_t size, size_t ofs, const char *file_name);

#endif /* test/lib.h */
