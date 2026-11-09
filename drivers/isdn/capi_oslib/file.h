#ifndef _capi_file_h_
#define _capi_file_h_

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init capi_oslib_file_init(void);
#if defined(CONFIG_CAPI_OSLIB_MODULE)
void capi_oslib_file_cleanup(void);
#endif /*--- #if defined(CONFIG_CAPI_OSLIB_MODULE) ---*/
void capi_oslib_file_activate(void);


#endif /*--- #ifndef _capi_file_h_ ---*/
