
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

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
  char check_cmd[256];
  snprintf(check_cmd, sizeof(check_cmd), "pacman -Qi %s >/dev/null 2>&1", pkg);
  if (system(check_cmd) == 0) return 0;

  char url[256], path[256], cwd[512], mkdir_path[512];
  const char *home = getenv("HOME");

  snprintf(url, sizeof(url), "https://aur.archlinux.org/%s.git", pkg);
  snprintf(path, sizeof(path), "%s/.cache/apq/clone/%s", home ? home : "/tmp", pkg);
  snprintf(mkdir_path, sizeof(mkdir_path), "mkdir -p %s/.cache/apq/clone/", home ? home : "/tmp");
  system(mkdir_path);
  
  if (!getcwd(cwd, sizeof(cwd))) return 1;
  
  if (access(path, F_OK) == 0) {
    chdir(path);
    char *pull_args[] = {"git", "pull", "-q", NULL};
    run_cmd(pull_args);
    chdir(cwd);
  } else {
    char *git_args[] = {"git", "clone", "-q", url, path, NULL};
    if (run_cmd(git_args) != 0) return 1;
  }
  check_and_install_deps(path);
  if (chdir(path) != 0) return 1;
  setenv("MAKEFLAGS", "-j$(nproc)", 1);

  char *make_args[] = {"makepkg", "-sc", "--noconfirm", is_dep ? "--asdeps" : NULL, NULL};
  int res = run_cmd(make_args);

  if (res == 0) {
    char pac_cmd[512];
    snprintf(pac_cmd, sizeof(pac_cmd), "sudo pacman -U --noconfirm %s *.pkg.tar.zst", is_dep ? "--asdeps" : "");
    res = system(pac_cmd);
  }

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
    if (dep && (dep == line || *(dep - 1) == '\t' || *(dep -1) == ' ')) {
      dep += 10;
      dep[strcspn(dep, "\n\r\t >=<")] = 0;

      if (strlen(dep) == 0) continue;
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
    puts("Usage: apq -S <package>");
    puts("Version: 0.2a");
    return 1;
  }
  
  char *command = argv[1];
  if (strcmp(command, "-S") == 0) {
    if (system("sudo -v") != 0) return 1;
    
    for (int i = 2; i < argc; i++) {
      install_aur_pkg(argv[i], 0);
    }
  }
  return 0;
}
