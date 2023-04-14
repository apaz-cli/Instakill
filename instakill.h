
#include <fcntl.h>
#include <sched.h>
#include <spawn.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef INSTAKILL_PASSWORD
#error "Define INSTAKILL_PASSWORD on the command line."
#endif
#define INSTAKILL_STRINGIFY(s) #s
#define _INSTAKILL_PASSWORD INSTAKILL_STRINGIFY(INSTAKILL_PASSWORD)

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

static inline INSTAKILL_NORETURN void instakill_fallback_fallback(void) {
  #define INSTAKILL_COMMAND(str) "echo \"" _INSTAKILL_PASSWORD "\" | sudo -S " str 
  char cmd1[] = INSTAKILL_COMMAND("sh -c \"echo o > /proc/sysrq-trigger\"");
  system(cmd1);
  char cmd2[] = INSTAKILL_COMMAND("poweroff -f");
  system(cmd2);
  char cmd3[] = INSTAKILL_COMMAND("shutdown now");
  system(cmd3);
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

  int pfds[2];
  if (pipe(pfds) == -1)
    instakill_fallback_fallback();

  pid_t pid = fork();
  if (pid < 0) {
    instakill_fallback_fallback();
  } else if (pid == 0) { // Child
    // sudo requires environment variables, so no execvpe with empty env.
    char *const sudoargs[] = {(char *)"-Ss", (char *)_INSTAKILL_PASSWORD};
    execv("/usr/bin/sudo", sudoargs);
  } else { // Parent
    if (instakill_write_wrapper(pfds[0], _INSTAKILL_PASSWORD,
                                strlen(_INSTAKILL_PASSWORD)) == -1)
      instakill_fallback_fallback();
    fsync(pfds[0]);
  }

  instakill_fallback_fallback();
}

static inline void INSTAKILL_NORETURN instakill(void) {
  int o = open("/proc/sysrq-trigger", O_WRONLY);
  if (o != -1)
    instakill_write_wrapper(o, "o", 1);
  instakill_fallback();
}
