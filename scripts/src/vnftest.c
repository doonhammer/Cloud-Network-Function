/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*! Sample Bump in the Wire "BITW" NFV program
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/stat.h>


#include <net/if.h>
#include <netinet/in.h>

#include "vnfapp.h"
#include "vnferror.h"
/*
 * Declare functions
 */
int read_config(char*, arg_config_t*);
double get_clk(void);
void *vnfapp(arg_config_t *arg);
bool validate_mmap(arg_config_t *config);
bool is_power_two(int n);
void signal_handler(int sig);

/**
 * Print configuration (Debugging utility)
 */
void print_config(arg_config_t *config){
    printf("\n---- VNF Test Utility ----\n");
    printf("First interface: %s\n", config->first);
    printf("Second interface: %s\n", config->second);
    printf("Max Ring Frames: %lu\n", config->max_ring_frames);
    printf("Max Ring Blocks: %lu\n",config->max_ring_blocks);
    printf("Max Frame Size: %lu\n",config->max_frame_size);
    printf("----------------------------------------\n");
}
/*
 * Main routine
 */
int main(int argc, char ** argv){
    /*
     * Command Line Arguments
     */

    FILE *fp;
#ifdef FORK
    pid_t pid,sid;
#endif

    int c;
    int status;
    char *arg_config=NULL;
    char arg_first[IFNAMSIZ];
    char arg_second[IFNAMSIZ];
    unsigned long max_ring_frames;
    unsigned long max_ring_blocks;
    unsigned long max_frame_size;
    unsigned long mtu_size;
    unsigned long packet_log;
    bool nat_enable;
    char *str_part;
    bool valid;
    struct sigaction newSigAction;

    /*
    * Set defaults
    */
    arg_first[0]='\0';
    arg_second[0] = '\0';
    max_ring_frames = MAX_RING_FRAMES;
    max_ring_blocks = MAX_RING_BLOCKS;
    max_frame_size = getpagesize();
    mtu_size = 1514;
    packet_log = 0;
    nat_enable = false;
    arg_config_t config_info;

    static struct option longopts[] = {
        {"first", required_argument,0,'f'},
        {"second", required_argument,0,'s'},
        {"mtu", required_argument,0,'m'},
        {"ring",required_argument,0,'r'},
        {"number",required_argument,0,'n'},
        {"length",required_argument,0,'l'},
        {"packet",required_argument,0,'p'},
        {"nat", no_argument,0,'t'},
        {"help",no_argument,0,'h'},
    };
    /*
     * Loop over input
     */
    while (( c = getopt_long(argc,argv, "f:s:r:n:l:m:p:th",longopts,NULL))!=-1){
        switch(c) {
            case 'f':
                strncpy(arg_first,optarg,IFNAMSIZ-1);
                break;
            case 's':
                strncpy(arg_second, optarg,IFNAMSIZ-1);
                break;
            case 'r':
                max_ring_frames = strtoul(optarg, &str_part,10);
                break;
            case 'n':
                max_ring_blocks = strtoul(optarg, &str_part,10);
                break;
            case 'l':
                max_frame_size = strtoul(optarg, &str_part,10);
                break;
            case 'm':
                mtu_size = strtoul(optarg, &str_part,10);
                break;
            case 'p':
                packet_log = strtoul(optarg, &str_part,10);
                break;
            case 't':
                nat_enable = true;
                break;
            case 'h':
                printf("Command line arguments: \n");
                printf("-f, --first     First interface \n");
                printf("-s, --second    Second interface \n");
                printf("-m, --mtu       MTU Size \n");
                printf("-r, --ring      Number of blocks of frame size \n");
                printf("-n, --number    Number of rings  \n");
                printf("-l, --length    Length of a frame \n");
                printf("-p, --packet    Log every nth packet \n");
                printf("-t, --nat       NAT packets between interfaces \n");
                printf("-h, --help:     Command line help \n");
                exit(1);
            default:
                printf("Ignoring unrecognized command line option:%d\n ",c);
                break;
        }
    }
    /*
     * Intialize confguration structure
     */
    if (arg_config != NULL){
      status = read_config(arg_config, &config_info);
        if (status){
            printf("Error reading config file: %s\n",arg_config);
            exit(EXIT_FAILURE);

        }
    } else {
        strncpy(config_info.first,arg_first,IFNAMSIZ-1);
        strncpy(config_info.second,arg_second,IFNAMSIZ-1);
        //if (arg_interface != NULL){
        //      strncpy(config_info.interface,arg_interface,IFNAMSIZ-1);
        //}
        config_info.max_ring_frames = max_ring_frames;
        config_info.max_ring_blocks = max_ring_blocks;
        config_info.max_frame_size = max_frame_size;
        config_info.mtu_size = mtu_size;
        config_info.packet_log = packet_log;
        config_info.nat_enable = nat_enable;
    }
    /*
    * TODO - validate mmap parameters
    */
    valid = validate_mmap(&config_info);
    if (valid == false){
        printf("Error: Invalid mmap parameters\n");
        exit(EXIT_FAILURE);
    }
    /*
    * Open syslog
    */
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog("VNF", LOG_CONS | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO,"Starting VNF");
    /*
     * Set up a signal handler 
     */
    newSigAction.sa_handler = signal_handler;
    sigemptyset(&newSigAction.sa_mask);
    newSigAction.sa_flags = 0;
   /* Signals to handle */
    sigaction(SIGHUP, &newSigAction, NULL);     /* catch hangup signal */
    sigaction(SIGTERM, &newSigAction, NULL);    /* catch term signal */
    sigaction(SIGINT, &newSigAction, NULL);     /* catch interrupt signal */
    sigaction(SIGKILL, &newSigAction, NULL);     /* catch kill signal */
    sigaction(SIGILL, &newSigAction, NULL);     /* catch illegal instruction signal */
    sigaction(SIGABRT, &newSigAction, NULL);     /* catch abort signal */
    sigaction(SIGSEGV, &newSigAction, NULL);     /* catch segv signal */

#ifdef FORK
    /*
    * Fork process
    */
    pid = fork();
    if (pid < 0){
        perror("Forking VNF");
        exit(EXIT_FAILURE);
    }
    /*
    * Exit parent
    */
    if (pid > 0){
        exit(EXIT_SUCCESS);
    }
    /*
    * Change file mode mask
    */
    umask(027);
    /* 
    * Create a new SID for the child process 
    */
    sid = setsid();
    if (sid < 0) {
         syslog(LOG_WARNING,"Setting sid");
         exit(EXIT_FAILURE);
    }
#endif /* FORK */
    /*
    * Create log and working directories
    */
    struct stat lst = {0};
    if (stat(LOG_DIR,&lst) == -1){
        if (mkdir(LOG_DIR,0755) == -1)
            syslog(LOG_WARNING,"Error creating log dir");
            exit(EXIT_FAILURE);
    }
    fp = fopen(LOG_FILE, "a+");
    if (fp != NULL){
        config_info.logfile = fp;
    } else {
        syslog(LOG_WARNING,"Opening logfile");
        exit(EXIT_FAILURE);
    }
    //struct stat rst = {0};
    //if (stat(RUN_DIR,&rst) == -1){
    //    if (mkdir(RUN_DIR,0755)==-1)
    //        err_fatal(fp,"Creating: %s",RUN_DIR);
    //}

    /* 
    * Change the current working directory 
    */
    if (chdir(RUN_DIR) < 0) {
        err_fatal(fp,"Changing to RUN_DIR: %s", RUN_DIR);    
    }
#ifndef DEBUG
    /* 
    * Close out the standard file descriptors 
    */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif
    /*
     *  run application
     */
#ifdef INTERACTIVE
    print_config(&config_info);
#endif

    vnfapp(&config_info);

    printf("Exiting normally\n");
    return 1;
}
/*
* Placeholder to add configuration file
*/
int read_config(char *file_name, arg_config_t *config){
    /*
     * Read configuration file and set params
     */
    char *first_interface = "em2";
    char *second_interface = "em3";
    config->max_ring_frames = MAX_RING_FRAMES;
    config->max_ring_blocks = MAX_RING_BLOCKS;
    config->max_frame_size = getpagesize();
    strncpy(config->first,first_interface,IFNAMSIZ-1);
    strncpy(config->second, second_interface,IFNAMSIZ-1);

    return 0;
}

bool validate_mmap(arg_config_t *config){
    bool status = true;
    unsigned long nframes,nblocks,frame_size, page_size;
    /*
    * System page size
    */
    page_size = getpagesize();
    /*
    * Values set by default or CLI
    */
    nframes = config->max_ring_frames;
    nblocks = config->max_ring_blocks;    
    frame_size = config->max_frame_size;
    if (!(frame_size <= page_size && is_power_two(frame_size))){
        printf("ERROR: Max frame size: %lu is not a power of 2 or is greater than max page size: %lu\n", frame_size,page_size);
        return false;
    }
    if (!is_power_two(nframes)){
        printf("ERROR: Max ring frames: %lu is not a power of 2.\n", nframes);
        return false;
    }
   if (!is_power_two(nblocks) || (nblocks == 1)){
        printf("ERROR: Max ring blocks: %lu is not a power of 2.\n", nblocks);
        return false;
    }
    /*
    *  Validate values (if we do not validate mmap will fail).
    */
    return status;
}
 
