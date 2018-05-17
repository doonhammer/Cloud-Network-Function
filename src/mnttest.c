#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>

int main(void)
{
  struct mntent *ent;
  FILE *aFile;

  aFile = setmntent("/etc/mtab", "r");
  if (aFile == NULL) {
    perror("setmntent");
    exit(1);
  }
  while (NULL != (ent = getmntent(aFile))) {
    //if (hasmntopt(ent,"/dev/shm") != NULL){
    if (strcmp(ent->mnt_dir,"/dev/shm") == 0) {
      printf("%s %s %s\n", ent->mnt_fsname, ent->mnt_dir, ent->mnt_opts);
    }
  }
  endmntent(aFile);
}
