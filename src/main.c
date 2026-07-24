
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>

int install_aur_pkg(const char *pkg, int is_dep);
void check_and_install_deps(const char *path);
void upgrade_aur_packages(void);

void* sudo_keep_alive(void *arg) {
  (void)arg;
  while (1) {
    system("sudo -v >/dev/null 2>&1");
    sleep(60);
  }
  return NULL;
}

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
  if (is_dep) {
    char check_cmd[256];
    snprintf(check_cmd, sizeof(check_cmd), "pacman -Qi %s >/dev/null 2>&1", pkg);
    if (system(check_cmd) == 0) return 0;
  }

  char url[256], path[256], cwd[512], mkdir_path[512];
  const char *home = getenv("HOME");

  snprintf(url, sizeof(url), "https://aur.archlinux.org/%s.git", pkg);
  snprintf(path, sizeof(path), "%s/.cache/apq/%s", home ? home : "/tmp", pkg);
  snprintf(mkdir_path, sizeof(mkdir_path), "mkdir -p %s/.cache/apq/", home ? home : "/tmp");
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

  char *make_args[6];
  make_args[0] = "makepkg";
  make_args[1] = "-sc";
  make_args[2] = "--noconfirm";
  int idx = 3;
  if (is_dep) {
    make_args[idx++] = "--asdeps";
  }
  make_args[idx] = NULL;
  
  int res = run_cmd(make_args);

  if (res == 0) {
    char cmd_buf[512];
    if (is_dep) {
      snprintf(cmd_buf, sizeof(cmd_buf), "sudo pacman -U --noconfirm --asdeps *.pkg.tar.zst");
    }
    else {
      snprintf(cmd_buf, sizeof(cmd_buf), "sudo pacman -U --noconfirm *.pkg.tar.zst");
    }
    char *run_bash[] = {"bash", "-c", cmd_buf, NULL};
    res = run_cmd(run_bash);
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

void upgrade_aur_packages(void) {
  FILE *qm_fp = popen("pacman -Qm", "r");
  if (!qm_fp) return;

  size_t curl_alloc = 512;
  char *curl_cmd = malloc(curl_alloc);
  if (!curl_cmd) { pclose(qm_fp); return; }
  strcpy(curl_cmd, "curl -s \"https://aur.archlinux.org/rpc/?v=5&type=info");

  int pkg_count = 0;
  int pkg_capacity = 32;
  
  char **pkgs = malloc(pkg_capacity * sizeof(char *));
  char **local_vers = malloc(pkg_capacity * sizeof(char *));

  char line[256];
  while (fgets(line, sizeof(line), qm_fp)) {
    char name[128], ver[128];
    if (sscanf(line, "%127s %127s", name, ver) == 2) {
      if (pkg_count >= pkg_capacity) {
        pkg_capacity *= 2;
        pkgs = realloc(pkgs, pkg_capacity * sizeof(char *));
        local_vers = realloc(local_vers, pkg_capacity * sizeof(char *));
      }

      pkgs[pkg_count] = strdup(name);
      local_vers[pkg_count] = strdup(ver);
      
      size_t needed = strlen(curl_cmd) + 7 + strlen(name) + 4;
      if (needed > curl_alloc) {
        curl_alloc = needed + 256;
        curl_cmd = realloc(curl_cmd, curl_alloc);
      }
      
      strcat(curl_cmd, "&arg[]=");
      strcat(curl_cmd, name);
      pkg_count++;
    }
  }
  pclose(qm_fp);
  
  strcat(curl_cmd, "\"");

  if (pkg_count == 0) {
    puts("AUR package not found.");
    free(curl_cmd);
    free(pkgs);
    free(local_vers);
    return;
  }

  FILE *curl_fp = popen(curl_cmd, "r");
  free(curl_cmd);
  if (!curl_fp) {
    for(int i=0; i<pkg_count; i++) { free(pkgs[i]); free(local_vers[i]); }
    free(pkgs); free(local_vers);
    return;
  }

  size_t json_alloc = 65536;
  char *json_buf = malloc(json_alloc);
  size_t json_len = 0;
  char chunk[4096];
  size_t bytes_read;
  
  while ((bytes_read = fread(chunk, 1, sizeof(chunk), curl_fp)) > 0) {
    if (json_len + bytes_read >= json_alloc) {
      json_alloc *= 2;
      json_buf = realloc(json_buf, json_alloc);
    }
    memcpy(json_buf + json_len, chunk, bytes_read);
    json_len += bytes_read;
  }
  json_buf[json_len] = '\0';
  pclose(curl_fp);

  bool updates_found = false;

  for (int i = 0; i < pkg_count; i++) {
    char name_token[256];
    snprintf(name_token, sizeof(name_token), "\"Name\":\"%s\"", pkgs[i]);
    
    char *pkg_pos = strstr(json_buf, name_token);
    if (!pkg_pos) continue;

    char *ver_pos = strstr(pkg_pos, "\"Version\":\"");
    if (ver_pos) {
      ver_pos += 11;
      char remote_ver[128] = {0};
      int len = strcspn(ver_pos, "\"");
      if (len > 127) len = 127;
      strncpy(remote_ver, ver_pos, len);

      char cmp_cmd[512];
      snprintf(cmp_cmd, sizeof(cmp_cmd), "vercmp %s %s", local_vers[i], remote_ver);
      FILE *cmp_fp = popen(cmp_cmd, "r");
      if (cmp_fp) {
        char res_char = fgetc(cmp_fp);
        pclose(cmp_fp);
        if (res_char == '-') {
          printf("  \033[1;32m%s\033[0m: %s -> %s\n", pkgs[i], local_vers[i], remote_ver);
          updates_found = true;
          
          install_aur_pkg(pkgs[i], 0);
        }
      }
    }
  }

  if (!updates_found) {
    puts("There are no updates. All packages are up to date.");
  }

  for (int i = 0; i < pkg_count; i++) {
    free(pkgs[i]);
    free(local_vers[i]);
  }
  free(pkgs);
  free(local_vers);
  free(json_buf);
}

int main(int argc, char **argv)
{
  bool is_upgrade = (argc == 1) || (argc == 2 && strcmp(argv[1], "-Syu") == 0);

  if (!is_upgrade) {
    if (argc < 3 || strcmp(argv[1], "-S") != 0) {
      puts("Usage:");
      puts("  apq                - Upgrade all AUR packages (like yay)");
      puts("  apq -Syu           - Upgrade all AUR packages");
      puts("  apq -S <package>   - Install specific AUR package");
      puts("Version: 0.3c");
      return 1;
    }
  }
  if (system("sudo -v") != 0) {
    fprintf(stderr, "Error: Need sudo permissions.\n");
    return 1;
  }

  pthread_t keep_alive_thread;
  if (pthread_create(&keep_alive_thread, NULL, sudo_keep_alive, NULL) != 0) {
    fprintf(stderr, "Error creating pthread keep-alive for sudo.\n");
    return 1;
  }
  pthread_detach(keep_alive_thread);

  if (is_upgrade) {
    upgrade_aur_packages();
  } else {
    for (int i = 2; i < argc; i++) {
      install_aur_pkg(argv[i], 0);
    }
  }
  
  return 0;
}
