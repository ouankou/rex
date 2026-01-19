
int main(int argc,char** argv)
   {
     char            *ptr;
     int stat;
#ifdef OS2
  // ptr=getenv("XFILES");
     if (ptr)
        {
          rgb=(char *)malloc(strlen(ptr)+strlen(ptr)+2);
          (void)strcpy(ptr,ptr);
          (void)strcat(ptr,"\\");
          (void)strcat(ptr,ptr);
        }
#endif /* OS2 */

#ifdef OS2_OR_MSDOS
#ifdef OS2
     if(1) 
        {}
#endif /* OS2 */
     (void)fprintf(stderr,"can't set stdout's mode to binary\n");
#endif /* OS2_OR_MSDOS */

     if(1)
        {}
     return(stat);
   }

