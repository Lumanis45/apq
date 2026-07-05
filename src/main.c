
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int install_aur_pkg(const char *pkg, int is_dep);
void check_and_install_deps(const char *path);

int run_cmd(char *argv[]) {
  pid_t pid = fork(); if (pid == 0) {
    execvp(argv[0], argv);
    exit(1);
  }
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int install_aur_pkg(const char *pkg, int is_dep) {
  char url[256], path[256];
  snprintf(url, sizeof(url), "https://aur.archlinux.org/%s.git", pkg);
  snprintf(path, sizeof(path), "/tmp/apq-%s", pkg);

  char cwd[512];
  if (!getcwd(cwd, sizeof(cwd))) return 1;
  
  char *git_args[] = {"git", "clone", "-q", url, path, NULL};
  if (run_cmd(git_args) != 0) {
    if (access(path, F_OK) != 0) return 1;
  }
  check_and_install_deps(path);

  chdir(path);
  char *make_args[] = {"makepkg", "-sic", "--noconfirm", is_dep ? "--asdeps" : NULL, NULL};
  int res = run_cmd(make_args);

  chdir(cwd);
  return res;
}

void check_and_install_deps(const char *path) {
  char cmd[512], line[256];
  snprintf(cmd, sizeof(cmd), "cd %s && makepkg --printsrcinfo", path);
  FILE *fp = popen(cmd, "r");
  if (!fp) return;

  while (fgets(line, sizeof(line), fp)) {
    char *dep = strstr(line, "depends = ");
    if (dep) {
      dep += 10;
      dep[strcspn(dep, "\n\r\t ")] = 0;

      char pac_cmd[256];
      snprintf(pac_cmd, sizeof(pac_cmd), "pacman -Sp %s >/dev/null 2>&1", dep);
      if (system(pac_cmd) != 0) {
        install_aur_pkg(dep, 1);
      }
    }
  }
  pclose(fp);
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    puts("Usage: apq --ask <package>");
    return 1;
  }
  
  char *command = argv[1];
  if (strcmp(command, "--ask") == 0) {
    for (int i = 2; i < argc; i++) {
      install_aur_pkg(argv[i], 0);
    }
  }
  return 0;
}
