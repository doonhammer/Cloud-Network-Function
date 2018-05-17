#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>


#include <sys/mount.h>
/*
* Appplication to mount /dev/shm of specific size on linux system
*/
int main(int argc, char **argv)
{
	FILE *out, *in;
	struct mntent *m, shm_m;
	unsigned int shm_size=0;
	char *str_part;
	char shm_opt[128];
	int c;

	static struct option longopts[] = {
		{"memory", required_argument,0,'m'},
		{"help",no_argument,0,'h'},
	};
    /*
     * Loop over input
     */
	while (( c = getopt_long(argc,argv, "m:h",longopts,NULL))!=-1){
		switch(c) {
			case 'm':
			shm_size = strtoul(optarg, &str_part,10);
			break;
			case 'h':
			printf("Command line arguments: \n");
			printf("-m, --memory    Shared memory (in Kib) to allocate \n");
			printf("-h, --help:     Command line help \n");
			exit(EXIT_SUCCESS);
			default:
			printf("Ignoring unrecognized command line option:%d\n ",c);
			break;
		}
	}

	if (shm_size == 0){
		printf("Shared memory %uKb - invalid value\n", shm_size);
		exit(EXIT_FAILURE);
	}
	if( access( "/etc/fstab.new", F_OK ) != -1 ) {
		if (remove("/etc/fstab.new") != 0){
			perror("Deleting /etc/fstab.new");
			exit(EXIT_FAILURE);
		}
	}
	/*
	* TODO add checks to ensure shm not greater than available memory
	*/
	if (snprintf(shm_opt,127,"defaults,size=%uk",shm_size) < 0){
		perror("Creating shm opts");
		exit(EXIT_FAILURE);
	}
	/*
	* New shared memory entry */
	shm_m.mnt_fsname = "tmpfs";
	shm_m.mnt_dir = "/dev/shm";
	shm_m.mnt_type = "tmpfs";
	shm_m.mnt_opts =shm_opt;
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
	while ((m = getmntent(in)) != NULL) { 
		if (strcmp(m->mnt_dir,"/dev/shm") == 0) {
			/* Do not copy existing shared memory entry */
		} else {
			addmntent(out, m); 
		}
	}
	/*
	* Add new shm entry
	*/
	addmntent(out,&shm_m);
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
//if (mount(NULL, "/dev/shm", NULL, MS_REMOUNT, NULL) == -1) {
//    perror("Mounting /etc/fstab");
//    exit(EXIT_FAILURE);
//}
/*
* For some reason remount using mount(2) does not work using cli instead
*/
	system("mount -o remount /dev/shm");

}

