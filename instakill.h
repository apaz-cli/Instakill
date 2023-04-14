
#include <fcntl.h>
#include <sched.h>
#include <spawn.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define INSTAKILL_PASSWORD "5461"

#if __STDC_VERSION__ < 201112L
#define INSTAKILL_NORETURN
#elif __STDC_VERSION__ >= 201112L && __STDC_VERSION__ <= 201710L
#define INSTAKILL_NORETURN _Noreturn
#else
#define INSTAKILL_NORETURN [[noreturn]]
#endif

#define INSTAKILL_ERROR(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  } while (0)

#define do_exit(smt)                                                           \
  do {                                                                         \
    smt;                                                                       \
    exit(0);                                                                   \
  } while (0)

static inline INSTAKILL_NORETURN void instakill_fallback_fallback(void) {
  system("echo " INSTAKILL_PASSWORD " | sudo -Ss echo o > /proc/sysrq-trigger");
  _exit(0);
}

static inline int instakill_write_wrapper(int fd, const void *mem,
                                          size_t bytes) {
  size_t written_sofar = 0;
  while (bytes) {
    ssize_t bytes_written = write(fd, (char *)mem + written_sofar, bytes);
    if (bytes_written == -1 && errno == EINTR)
      continue;
    if (bytes_written == -1 && errno != EINTR)
      return -1;
    bytes -= bytes_written;
    written_sofar += bytes_written;
  }
  return 0;
}

static inline INSTAKILL_NORETURN void instakill_fallback(void) {

  int pfds[2]; // [0] for read, [1] for write.
  if (pipe(pfds) == -1)
    instakill_fallback_fallback();

  pid_t pid = fork();
  if (pid < 0) {
    instakill_fallback_fallback();
  } else if (pid == 0) { // Child
    pid_t sudo_pid = 0;
    char *const sudoargs[] = {(char *)"-Ss", (char *)INSTAKILL_PASSWORD};
    // sudo requires environment variables, so no execvpe with empty env.
    execv("/usr/bin/sudo", sudoargs);
  } else { // Parent
    if (instakill_write_wrapper(pfds[0], INSTAKILL_PASSWORD,
                                strlen(INSTAKILL_PASSWORD)) == -1)
      instakill_fallback_fallback();
    fsync(pfds[0]);
  }

  instakill_fallback_fallback();
}

static inline void instakill(void) {
  int o = open("/proc/sysrq-trigger", O_RDONLY);
  if (o == -1)
    instakill_fallback();
  int w = instakill_write_wrapper(o, "o", 1);
  if (w == -1)
    instakill_fallback();
  _exit(0);
}
