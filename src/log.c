#include <stdlib.h>
#include <stdio.h>
#include <time.h>

char g_time_buffer[64];

void _get_time_str(char* buff) {
    time_t t ;
    struct tm *tmp ;
    time( &t );    
    tmp = localtime( &t );
    
    // using strftime to display time
    strftime(buff, 64, "%x - %I:%M%p", tmp);
}