/*

Dan:
        Thanks. I'm not sure I've explained the problem clearly. The
issue is that in ROSE 0.8.0b the GNU_EXTENSIONS_ALLOWED macro within
legacy frontend is turned off.  I turned it on but then ROSE would not compile.

../../../../ROSE-0.8.0b/src/frontend/legacy_frontend/sage_gen_be.C:73:2: #error
"GNU_EXTENSIONS_ALLOWED is not currently defined in SAGE III (later)!"

        I have a copy of legacy frontend (3.3) separate from ROSE that I
obtained early in my work before we knew about ROSE.  Setting the
GNU_EXTENSIONS_ALLOWED macro to TRUE (and using the --gcc command line option)
in that code allows legacy frontend (again, separate from ROSE, via the eccp
command) to parse the following source files without error.

        So I dont think this is a case of extending legacy frontend, but
allowing the legacy frontend that is used with ROSE to enable the
GNU_EXTENSIONS_ALLOWED macro.

thanks and sorry for the confusion.
chadd


TEST CASES:

Here is the command I use to run rose over each of these files:
mytool -rose:C   -c filename.c

Here are each of the files I've been using as test cases, all are
derived from source code i have seen in either Wine (winehq.com)
or Apache's httpd.

Each of these files compile with gcc 3.3.3
*/
