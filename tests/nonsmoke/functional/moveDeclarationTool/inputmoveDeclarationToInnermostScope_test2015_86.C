int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
const char *strstr(const char *, const char *);
char *strrchr(char *, int);
int isdigit(int);
int strlen(const char *);

const int MAXLINE = 42;

void foobar()
   {
     char rgdmpname[MAXLINE], cname[MAXLINE], arg1[16];
     char msg[MAXLINE], tmpstr[MAXLINE], tmpstr_lower[MAXLINE];

     char** argv;
     int i;
     int ierr;

     char* ptr;
     bool noerrflag;

     if (1)
        {     
          if (!strcmp(tmpstr_lower,"abc")) {
               strcpy(rgdmpname, argv[i+1]);
               i++;
            }
            else if (!strcmp(tmpstr_lower,"abc")) {

               strcpy(rgdmpname, argv[i+1]);
               i++;
            }
            else if (!strcmp(tmpstr_lower,"abc")) {

            }
            else if (!strcmp(tmpstr_lower,"abc")) {

            } else if ((strstr(tmpstr_lower, "abc") != 0) ||
                       (strstr(tmpstr_lower, "abc") != 0)) {

            } else if (((ptr = strrchr(tmpstr, '-')) != 0) &&
                       (isdigit((int)(ptr[1])))) {
              *ptr = '\0';
              strcpy(rgdmpname, tmpstr);
            } else if ((isdigit((int)(tmpstr[0]))) &&
                       (isdigit((int)(tmpstr[strlen(tmpstr) - 1])))) {

            } else {
            }
            if (ierr != 0) {
               rgdmpname[0] = '\0' ;
            }

         } else if (!strcmp(argv[i],"abc")) {

            noerrflag = true;

         }
   }
