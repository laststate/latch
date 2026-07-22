#if defined(_MSC_VER)
void _RTC_InitBase(void){}void _RTC_Shutdown(void){}void _RTC_CheckStackVars(void*frame,void*descriptor){(void)frame;(void)descriptor;}
#endif
void ls_freestanding_entry(void){for(;;){}}
