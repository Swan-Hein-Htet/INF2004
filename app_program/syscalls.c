/* Required newlib ABI names; application I/O uses T-Monitor, never stdio. */
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>

void *_sbrk(ptrdiff_t increment);
int _write(int handle, char const *p_buffer, int length);
int _read(int handle, char *p_buffer, int length);
int _close(int handle);
off_t _lseek(int handle, off_t offset, int origin);

void *_sbrk(ptrdiff_t increment)
{
    (void)increment;
    errno = ENOMEM;
    return (void *)-1;
}

int _write(int handle, char const *p_buffer, int length)
{
    (void)handle;
    (void)p_buffer;
    (void)length;
    errno = EBADF;
    return -1;
}

int _read(int handle, char *p_buffer, int length)
{
    (void)handle;
    (void)p_buffer;
    (void)length;
    errno = EBADF;
    return -1;
}

int _close(int handle)
{
    (void)handle;
    errno = EBADF;
    return -1;
}

off_t _lseek(int handle, off_t offset, int origin)
{
    (void)handle;
    (void)offset;
    (void)origin;
    errno = EBADF;
    return (off_t)-1;
}
