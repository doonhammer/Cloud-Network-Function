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
#include "vnferror.h"

#define MAXLINE 4096
#define LISTENQ 1024

static void err_handler(FILE *fp,int level, const char *fmt, va_list ap){
	unsigned long errno_save, n;
	char buf[MAXLINE + 1];
	errno_save = errno;

	switch 	(level) {
		case VNF_LOG_INFO:
			strcpy(buf,"\nINFORMATIONAL: ");
			break;
		case VNF_LOG_ERR:
			strcpy(buf,"\nERROR: ");
			break;
		default:
			break;
	}
	n = strlen(buf);
	vsnprintf(buf+n, MAXLINE - n,fmt,ap);
	n = strlen(buf);
	snprintf(buf + n, MAXLINE - n, ": %s\n", strerror(errno_save));
	fprintf(fp,buf);
	fflush(fp);
	return;
}
void err_fatal(FILE *fp, const char *fmt, ...){
	va_list ap;
	va_start(ap,fmt);
	err_handler(fp,VNF_LOG_ERR, fmt,ap);
	va_end(ap);
	fclose(fp);
	exit(EXIT_FAILURE);
}

void err_info(FILE *fp, const char *fmt, ...){
	va_list ap;
	va_start(ap,fmt);
	err_handler(fp,VNF_LOG_INFO, fmt,ap);
	va_end(ap);
	return;
}

