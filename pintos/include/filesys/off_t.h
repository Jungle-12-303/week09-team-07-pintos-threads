#ifndef FILESYS_OFF_T_H
#define FILESYS_OFF_T_H

#include <stdint.h>

/* An offset within a file.
 * This is a separate header because multiple headers want this
 * definition but not any others. */
/* 파일 내 오프셋입니다.
 * 여러 헤더에서 이 정의를 필요로 하지만 다른 정의는 필요로 하지 않으므로 별도의 헤더로 정의합니다. */
typedef int32_t off_t;

/* Format specifier for printf(), e.g.:
 * printf ("offset=%"PROTd"\n", offset); */
#define PROTd PRId32

#endif /* filesys/off_t.h */