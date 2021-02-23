#include <iostream>
#include <pty.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

void log(const char *str, int len)
{
    FILE *fh = fopen("/tmp/shlang.log", "a+");
	fwrite(str, 1, len, fh);
	fclose(fh);
}

void openpty_demo(const char *host) {
  int master;
  int slave;
  openpty(&master, &slave, NULL, NULL, NULL);

  // Temporarily redirect stdout to the slave, so that the command executed in
  // the subprocess will write to the slave.
  int _stdout = dup(STDOUT_FILENO);
  dup2(slave, STDOUT_FILENO);
  int _stdin = dup(STDIN_FILENO);
  dup2(slave, STDIN_FILENO);

  pid_t pid = fork();
  if (pid == 0) {
	const char *argv2[] = {"ssh", "-o", "StrictHostKeyChecking=no", host, NULL};
    execvp(argv2[0], const_cast<char *const *>(argv2));
  }

  pid_t pid1 = fork();
  if (!pid1) {
      while (true) {
          sleep(1);
		  write(master, "ls /\n", 5);
      }
  }

  fd_set rfds;
  struct timeval tv{0, 0};
  char buf[4097];
  ssize_t size;
  size_t count = 0;

  // Read from master as we wait for the child process to exit.
  //
  // We don't wait for it to exit and then read at once, because otherwise the
  // command being executed could potentially saturate the slave's buffer and
  // stall.
  while (1) {
    if (waitpid(pid, NULL, WNOHANG) == pid) {
      break;
    }

    FD_ZERO(&rfds);
    FD_SET(master, &rfds);
    if (select(master + 1, &rfds, NULL, NULL, &tv)) {
      size = read(master, buf, 4096);
      buf[size] = '\0';
      count += size;
      log(buf, size);
    }
  }

  // Child process terminated; we flush the output and restore stdout.
  fsync(STDOUT_FILENO);
  dup2(_stdout, STDOUT_FILENO);

  fsync(STDIN_FILENO);
  dup2(_stdin, STDIN_FILENO);

  // Close both ends of the pty.
  close(master);
  close(slave);
}

int main(int argc, const char *argv[]) {
  openpty_demo(argv[1]);
}