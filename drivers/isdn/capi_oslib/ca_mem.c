
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#pragma GCC pop_options

#include <avm/capi_oslib/oslib.h>
#if __has_include(<avm/pcmlink/ul.h>)
#include <avm/pcmlink/ul.h>
#else
#include <linux/pcmlink_ul.h>
#endif
#include <asm/atomic.h>
#include "debug.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/* #define CHECK_ALLOC */

#if defined(CHECK_ALLOC)
#define MALLOC_KENNUNG_START        0x56785AA5
#define MALLOC_KENNUNG_END          0x1234A55A
#endif /*--- #if defined(CHECK_ALLOC) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CHECK_ALLOC)
static atomic_t alloc_value;
static atomic_t alloc_open;
static atomic_t alloc_open_max;

#define N_ALLOC_REFS 1024

static void *alloc_refs[N_ALLOC_REFS];

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void alloc_ref_add( void *ref )
{
	unsigned int n = 0;
	static unsigned int i = 0;

	if ( ref == NULL )
	{
		DEB_ERR( "{%s} error: invalid pointer\n", __func__ );
		return ;
	}
	atomic_inc( &alloc_open );
	if ( atomic_read( &alloc_open ) > atomic_read( &alloc_open_max ))
		atomic_inc( &alloc_open_max );
	while ( n <  N_ALLOC_REFS )
	{
		EnterCritical( );
		if ( alloc_refs[i] == NULL )
		{
			/* DEB_TRACE( "{%s} 0x%p (%u)\n", __func__, ref, i );  */
			alloc_refs[i] = ref ;
			LeaveCritical( );
			i++;
			return  ;
		}
		LeaveCritical( );
		n++;
		if ( ++i >= N_ALLOC_REFS )
			i = 0;
	}
	DEB_ERR( "{%s} error: no resource ...\n", __func__ );
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void alloc_ref_del( void *ref )
{
	int i = 0;

	if ( ref == NULL )
	{
		DEB_ERR( "{%s} error: invalid pointer\n", __func__ );
		return ;
	}
	if ( 0 == atomic_read( &alloc_open ))
	{
		DEB_ERR( "{%s} error: invalid action\n", __func__ );
		return ;
	}
	atomic_dec( &alloc_open );

	i = 0;
	while ( i <  N_ALLOC_REFS )
	{
		EnterCritical(  );
		if ( alloc_refs[i] == ref )
		{
			alloc_refs[i] = NULL;
			/* DEB_TRACE( "{%s} 0x%p (%u)\n", __func__, ref, i );  */
			LeaveCritical( );
			return ;
		}
		LeaveCritical( );
		i++;
	}
	DEB_ERR( "{%s} error: not in list 0x%p (%s:%d)\n", __func__, ref,
			(char*)(( unsigned int * )ref)[2],
			( unsigned int )(( unsigned int * )ref)[3]);
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void alloc_ref_print( void )
{
	int i = 0;
	while ( i <  N_ALLOC_REFS )
	{
		if ( alloc_refs[i] != NULL )
		{
			DEB_ERR( "{%s} still allocated 0x%p (%s:%d)\n", __func__, alloc_refs[i],
					(char*)(( unsigned int * )alloc_refs[i])[2],
					( unsigned int )(( unsigned int * )alloc_refs[i])[3]);
		}
		i++;
	}
}
#endif /* defined( CHECK_ALLOC ) */
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void *_CA_MALLOC(unsigned int size, char *File __attribute__((unused)), unsigned int Line __attribute__((unused))) {
	void *rtn;

	if(pcmlink_ul_assert_on_yield_context()) {
		return NULL;
	}
#if defined(CHECK_ALLOC)
	rtn = (void *)kmalloc(size + 5 * sizeof(unsigned int), GFP_ATOMIC);
	if(rtn) {
		size += 3; size &= ~0x03;
		((unsigned int *)rtn)[0]        = size;
		((unsigned int *)rtn)[1]        = MALLOC_KENNUNG_START;
		((unsigned int *)rtn)[2]        = (unsigned int)File;
		((unsigned int *)rtn)[3]        = Line;
		((unsigned int *)rtn)[(size >> 2) + 4] = MALLOC_KENNUNG_END;

		alloc_ref_add( rtn );

		rtn = (void *)((unsigned int *)rtn + 4);
	}
	/*--- DEB_TRACE("CA_MALLOC: %u bytes ptr=%x (%s, %d)\n", size, (unsigned int)rtn, File, Line); ---*/
	if(rtn) memset(rtn, 0, size);
	atomic_add((signed)size, &alloc_value);
#else /* defined(CHECK_ALLOC) */

	if( NULL != ( rtn = (void *)kmalloc(size, GFP_ATOMIC)))
	{
		memset(rtn, 0, size);
	} else {
		DEB_WARN("[CA_MALLOC] calloc %u bytes failed\n", size);
	}
#endif /* defined(CHECK_ALLOC) */
	return rtn;
}
EXPORT_SYMBOL(_CA_MALLOC);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void _CA_FREE(void *p, char *File __attribute__((unused)), unsigned int Line __attribute__((unused))) {
	if(pcmlink_ul_assert_on_yield_context()) {
		return;
	}
#if defined(CHECK_ALLOC)
	unsigned int size  = 0;

	if(p) {
		unsigned int Start;
		unsigned int Ende;
		char *a_File;
		unsigned int a_Line;

		p       = (void *)((unsigned int *)p - 4);
		size    = ((unsigned int *)p)[0];
		Start   = ((unsigned int *)p)[1];
		a_File  = (char *)(((unsigned int *)p)[2]);
		a_Line  = ((unsigned int *)p)[3];
		Ende    = ((unsigned int *)p)[(size >> 2) + 4];

		((unsigned int *)p)[1]                = 0; /*--- kennzeichen zurücksetzen ---*/
		((unsigned int *)p)[(size >> 2) + 4]  = 0; /*--- kennzeichen zurücksetzen ---*/

		atomic_sub((signed)size, &alloc_value);

		alloc_ref_del( p );

		if(Start != MALLOC_KENNUNG_START || Ende != MALLOC_KENNUNG_END) {
			DEB_ERR("_CA_FREE: StartKennung=%s EndeKennung=%s Size=%u (%s, %u) (alloc: %s, %u)\n",
				Start == MALLOC_KENNUNG_START ? "ok" : "error",
				Ende  == MALLOC_KENNUNG_END   ? "ok" : "error",
				size, File, Line, a_File, a_Line);
			return;
		}
	}
#endif /* defined(CHECK_ALLOC) */

	/*--- DEB_TRACE("CA_FREE: %u bytes %x (%s, %d)\n", size, (unsigned int)p, File, Line); ---*/
	if(p) {
		kfree(p);
	}
}
EXPORT_SYMBOL(_CA_FREE);
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void *CA_MALLOC(unsigned int size) {
	void *rtn;

	if(pcmlink_ul_assert_on_yield_context()) {
		return NULL;
	}
	if( NULL != ( rtn = (void *)kmalloc(size, GFP_ATOMIC)))
	{
		memset(rtn, 0, size);
	} else {
		DEB_WARN("[CA_MALLOC] calloc %u bytes failed\n", size);
	}
	return rtn;
}
EXPORT_SYMBOL(CA_MALLOC);
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_FREE(void *p ) {

	if(pcmlink_ul_assert_on_yield_context()) {
		return;
	}
	if(p) {
		kfree(p);
	}
}
EXPORT_SYMBOL(CA_FREE);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void CA_MEM_SHOW( void )
{
#if defined(CHECK_ALLOC)
	DEB_WARN( "{%s} memory in use: %u bytes (%u allocations, %u max. allocations) \n", __func__,
			atomic_read(&alloc_value),
			atomic_read(&alloc_open),
			atomic_read(&alloc_open_max));
#endif
}
EXPORT_SYMBOL(CA_MEM_SHOW);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_MEM_EXIT( void )
{
#if defined(CHECK_ALLOC)
	DEB_WARN( "{%s} %u bytes still allocated (%u allocations, %u max allocations )\n", __func__,
				atomic_read(&alloc_value),
				atomic_read(&alloc_open),
				atomic_read(&alloc_open_max));
	alloc_ref_print( );
#endif
}
EXPORT_SYMBOL( CA_MEM_EXIT );
