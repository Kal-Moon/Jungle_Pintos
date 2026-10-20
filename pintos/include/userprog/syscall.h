#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>

struct file;

void syscall_init (void);

/* 종료 상태를 남기고 프로세스를 끝낸다. 돌아오지 않는다.
   page_fault () 에서도 부르므로 여기서 선언한다. */
void sys_exit (int status) NO_RETURN;

/* 유저가 넘긴 주소 검사. 잘못된 주소면 sys_exit (-1). */
void validate_user_addr (const void *uaddr);
void validate_user_buffer (const void *buffer, unsigned size);
void validate_user_string (const char *str);

/* fd 테이블. (B) */
int fd_alloc (struct file *file);
struct file *fd_lookup (int fd);

#endif /* userprog/syscall.h */
