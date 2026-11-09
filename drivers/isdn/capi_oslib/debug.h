#ifndef _debug_h_
#define _debug_h_

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define OSLIB_DEBUG ---*/
/*--- #define OSLIB_MSGDEBUG ---*/

#ifdef DEB_ERR
#undef DEB_ERR
#endif
#ifdef DEB_WARN
#undef DEB_WARN
#endif
#ifdef DEB_NOTE
#undef DEB_NOTE
#endif
#ifdef DEB_INFO
#undef DEB_INFO
#endif
#ifdef DEB_TRACE
#undef DEB_TRACE
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define CAPI_OSLIB_STRING       "[capi_oslib] "
#if defined(OSLIB_DEBUG) 
#define DEB_ERR(args...)     printk(KERN_ERR CAPI_OSLIB_STRING args)
#define DEB_WARN(args...)    printk(KERN_WARNING CAPI_OSLIB_STRING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE CAPI_OSLIB_STRING args)
#define DEB_INFO(args...)    printk(KERN_INFO CAPI_OSLIB_STRING args)
#define DEB_TRACE(args...)   printk(KERN_INFO CAPI_OSLIB_STRING args)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#else /*--- #if defined(NDEBUG) ---*/
#define DEB_ERR(args...)	printk(KERN_ERR CAPI_OSLIB_STRING args)
#define DEB_WARN(args...)	no_printk(KERN_ERR CAPI_OSLIB_STRING args)
#define DEB_NOTE(args...)	no_printk(KERN_ERR CAPI_OSLIB_STRING args)
#define DEB_INFO(args...)	no_printk(KERN_ERR CAPI_OSLIB_STRING args)
#define DEB_TRACE(args...)	no_printk(KERN_ERR CAPI_OSLIB_STRING args)
#endif /*--- #else ---*/ /*--- #if defined(NDEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file;
struct _capi_oslib_open_data;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_register_open_data(struct _capi_oslib_open_data *open_data);
void capi_oslib_release_open_data(struct _capi_oslib_open_data *open_data);
int capi_oslib_dump_open_data(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *read_pos);
char *capi_oslib_capi_error_name(int);
void capi_oslib_dump_open_data_list(char *);

#endif /*--- #ifndef _debug_h_ ---*/
