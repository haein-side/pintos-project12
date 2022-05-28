#include <syscall.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) {
	exit (main (argc, argv)); // main 함수에 반환값이 생기면 exit()이 실행됨
}
