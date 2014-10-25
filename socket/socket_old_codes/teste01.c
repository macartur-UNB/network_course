/* Lab Redes 2 - Prof. Fernando W. Cruz    */
/* leituraescrita em arquivos*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc,char **argv) {
  int z, fd_orig, fd_dest;
  char getbuf[1];/* GET buffer */

  if (argc != 2) {
     printf("SINTAXE: %s <Nome_arquivo_a_ser_criado>\n", argv[0]);
     exit(0);
  }
  fd_orig = open("index.html", O_RDONLY, S_IRWXO );
  fd_dest = open(argv[1], O_RDWR|O_CREAT, S_IRWXO );
  while ( ( z = read (fd_orig, getbuf, 1 )) > 0 ) {
	z = write(fd_dest, getbuf, 1);
  }/* fim-while */
  printf("Arquivo %s criado ...\n", argv[1]);
  close(fd_orig); close(fd_dest);
 return 0;
} /* fim-main */
