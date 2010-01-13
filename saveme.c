/* 

	SAVEME - Part of the tsocks package
		 This program is designed to be statically linked so
		 that if a user breaks their ld.so.preload file and
		 cannot run any dynamically linked program it can 
		 delete the offending ld.so.preload file.

*/

#include <stdio.h>
#include <unistd.h>

int main() {

	unlink("/etc/ld.so.preload");

}
