
#include "systemDataStructures.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <openssl/sha.h> /* please read this */

#ifndef min
#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif /* ~min */

//#define SHA_DIGEST_LENGTH 20

unsigned char *GetUOID(
          char *node_inst_id,
          char *obj_type,
          unsigned char *uoid_buf,
          int uoid_buf_sz)
  {
      static unsigned long seq_no=(unsigned long)1;
      unsigned char sha1_buf[SHA_DIGEST_LENGTH];
      unsigned char str_buf[104];

      snprintf(reinterpret_cast<char*>(str_buf), sizeof(str_buf), "%s_%s_%1ld",node_inst_id, obj_type, (long)seq_no++);
      SHA1(str_buf, strlen(reinterpret_cast<const char*>(str_buf)), sha1_buf);
      memset(uoid_buf, 0, uoid_buf_sz);
      memcpy(uoid_buf, sha1_buf,min(uoid_buf_sz,(int)sizeof(sha1_buf)));

      //cout << "SHA DUMP: " <<  uoid_buf << " str_buf: " << str_buf << endl;
     /* cout << "\n=============================================================+++++++++";
      cout << "\n=============================================================+++++++++";
      printf("\nUOID CREATED IS: for %s\n",str_buf);
      int i=0;
      while(i < 20){
    	  printf("%02x ",*(uoid_buf + i));
    	  i++;
      }
      cout << "\n=============================================================+++++++++";
      cout << "\n=============================================================+++++++++\n";
*/
      return uoid_buf;
  }
