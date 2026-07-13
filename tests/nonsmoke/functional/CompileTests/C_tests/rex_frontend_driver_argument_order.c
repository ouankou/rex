#if !defined(REX_DRIVER_ARGUMENT_ORDER) || REX_DRIVER_ARGUMENT_ORDER != 2
#error REX_DRIVER_ARGUMENT_ORDER must reflect the final ordered -D option
#endif

#ifdef REX_DRIVER_ARGUMENT_UNDEFINED
#error REX_DRIVER_ARGUMENT_UNDEFINED must remain undefined after -U
#endif

int rex_frontend_driver_argument_order(void) {
  return REX_DRIVER_ARGUMENT_ORDER;
}
