// Example code from busy-box, use of enum in switch body before first case statement.

void foo()
   {
     int x;

     switch (x) {
       // Note enum type declaration at top of switch before first case statement.
       enum { CWD_LINK, EXE_LINK };
          case CWD_LINK:
               break;

          case EXE_LINK:
               break;

          default:
               break;
          }
   }
