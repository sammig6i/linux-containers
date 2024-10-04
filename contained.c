#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sched.h>
#include <seccomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <linux/capability.h>
#include <linux/limits.h>

// Structure to hold configuration for the child process
struct child_config {
  int argc;        // Number of arguments for the child process
  uid_t uid;       // User ID for the child process
  int fd;          // File descriptor
  char* hostname;  // Hostname for the child process
  char** argv;     // Arguments for the child process
  char* mount_dir; // Mount directory for the child process
};

int main(int argc, char** argv) {
  struct child_config config = { 0 }; // Initialize config structure with zeros
  int err = 0;                        // Error flag
  int option = 0;                     // Option for getopt
  int sockets[2] = { 0 };             // Array to hold socket file descriptors
  pid_t child_pid = 0;                // Process ID of the child
  int last_optind = 0;                // Last processed option index

  // Parse command-line options
  while ((option = getopt(argc, argv, "c:m:u:"))) {
    switch (option) {
    case 'c':
      // Set the command to be executed in the container
      config.argc = argc - last_optind - 1;
      config.argv = &argv[argc - config.argc];
      goto finish_options;
    case 'm':
      // Set the mount directory
      config.mount_dir = optarg;
      break;
    case 'u':
      // Set the user ID
      if (sscanf(optarg, "%d", &config.uid) != 1) {
        fprintf(stderr, "badly-formatted uid: %s\n", optarg);
        goto usage;
      }
      break;
    default:
      // Invalid option, show usage
      goto usage;
    }
    last_optind = optind;
  }

finish_options:
  // This label is reached when all options are processed or when 'c' option is found
  // Check if required options are provided
  if (!config.argc) goto usage;    // No command specified
  if (!config.mount_dir) goto usage; // No mount directory specified

  // Generate a hostname for the container
  char hostname[256] = { 0 };
  if (choose_hostname(hostname, sizeof(hostname)))
    goto error;
  config.hostname = hostname;

  // If execution reaches here, all setup was successful
  goto cleanup;

usage:
  // This label is reached when there's an error in command-line arguments
  // Print usage information
  fprintf(stderr, "Usage: %s -u -l -m . -c /bin/sh ~\n", argv[0]);
  // Fall through to error

error:
  // This label is reached when an error occurs during setup
  err = 1;
  // Fall through to cleanup

cleanup:
  // This label is always reached at the end of the program
  // Close any open sockets
  if (sockets[0]) close(sockets[0]);
  if (sockets[1]) close(sockets[1]);

  // The program will implicitly return 'err' here (0 for success, 1 for error)
  return err;
}