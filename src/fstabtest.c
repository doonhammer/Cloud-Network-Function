#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>

int filecopy(char *in, char *out);
int main(void)
{
	FILE *out, *in;
	struct mntent *m, shm_m;

	if( access( "/etc/fstab.new", F_OK ) != -1 ) {
	    if (remove("/etc/fstab.new") != 0){
			perror("Deleting /etc/fstab.new");
			exit(EXIT_FAILURE);
		}
	}
	/*
	* New shared memory entry */
	shm_m.mnt_fsname = "tmpfs";
	shm_m.mnt_dir = "/dev/shm";
	shm_m.mnt_type = "tmpfs";
	shm_m.mnt_opts = "defaults,size=1024m";
	shm_m.mnt_freq = 0;
	shm_m.mnt_passno = 0;
	/*
	* Get entry handle
	*/
	out = setmntent("/etc/fstab.new", "w+");
	if (out == NULL){
		perror("Opening /etc/fstab.new");
		exit(EXIT_FAILURE);
	}
/*
* open the original for input
*/
	in = setmntent("/etc/fstab", "r");
	if (in == NULL){
		perror("Opening /etc/fstab");
		exit(EXIT_FAILURE);
	}

/*
* copy the contents:
*/
//if (hasmntopt(m,"/dev/shm") != NULL){
//	printf("fstab has shared memory entry\n");
//
while (m = getmntent(in)) { 
    if (strcmp(m->mnt_dir,"/dev/shm") == 0) {
      addmntent(out, &shm_m);
    } else {
		addmntent(out, m); 
	}
}
/* 
* make sure the output has it all
*/
fflush(out); 
endmntent(out); 
endmntent(in);
/* 
* atomically replace /etc/fstab
*/
if ( rename("/etc/fstab.new", "/etc/fstab") != 0){
	perror("renaming /etc/fstab.new");
	exit(EXIT_FAILURE);
}
/*
 * Remount FSTAB to activate new /dev/shm
 */
if (mount(NULL, "/dev/shm", NULL, MS_REMOUNT, NULL) == -1) {
    perror("Mounting /etc/fstab");
    exit(EXIT_FAILURE);
}
//system("mount -o remount /dev/shm");

}

int filecopy(char *in, char * out){
	int status = 0;
	FILE *source, *dest;
	char ch;

	source = fopen(in,"r");
	if (source == NULL){
		perror("Opening");
		exit(EXIT_FAILURE);
	}
	dest = fopen(out,"w+");
	if (dest == NULL){
		perror("Opening");
		exit(EXIT_FAILURE);
	}
	while( ( ch = fgetc(source) ) != EOF )
      fputc(ch, dest);
 
    printf("File copied successfully.\n");
 
   fclose(source);
   fclose(dest);

   return status;
}
