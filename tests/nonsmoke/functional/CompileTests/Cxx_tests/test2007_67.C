
// This is a C99 specific bug, and works fine in C++.

void  alarm()
{
    enum {abort, scan} why = scan;

    while (why != abort)
    {
            why = abort; 
    }
}  

