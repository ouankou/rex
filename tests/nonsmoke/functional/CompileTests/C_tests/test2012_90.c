
int ngx_show_version = 1;
int ngx_show_help = 1;
int ngx_show_configure = 1;
int ngx_test_config = 1;
int ngx_quiet_mode = 1;
char* ngx_prefix;
char* ngx_conf_params;
char* ngx_conf_file;
char* ngx_signal;



int
ngx_get_options(int argc, char *const *argv)
{
    unsigned char *p;
    int i;

    for (i = 1; i < argc; i++) {
      //      while (*p) {

      switch (*p++) {

      case '?':
      case 'h':
        ngx_show_version = 1;
        ngx_show_help = 1;
        break;
      case 'v':
        ngx_show_version = 1;
        break;

      case 'V':
        ngx_show_version = 1;
        ngx_show_configure = 1;
        break;
      }
      //      }
      // #if 1
    next:
      continue;
// #endif
    }

    return 0;
}
