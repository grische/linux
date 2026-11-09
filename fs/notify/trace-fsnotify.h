#undef TRACE_SYSTEM
#define TRACE_SYSTEM fsnotify

#if !defined(_TRACE_FSNOTIFY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FSNOTIFY_H

#include <asm/bitsperlong.h>
#include <linux/fsnotify_backend.h>
#include <linux/tracepoint.h>

#ifndef FS_TRACED_EVENTS
#  define FS_TRACED_EVENTS (FS_EVENTS_POSS_ON_CHILD|FS_DELETE_SELF|FS_MOVE_SELF)
#endif

TRACE_EVENT(fsnotify_event,

	TP_PROTO(struct inode *to_tell, __u32 mask, const char *file_name),
	TP_ARGS(to_tell, mask, file_name),

	TP_STRUCT__entry(
		__field(unsigned long, watching_ino)
		__field(__u32, mask)
		__array(char, file_name, NAME_MAX)
	),

	TP_fast_assign(
		__entry->watching_ino = to_tell->i_ino;
		__entry->mask = mask;
		strlcpy(__entry->file_name, file_name ?: "(null)", NAME_MAX);
	),

	TP_printk("tell-ino=0x%0*lx path=\"%s\" event=%s",
		BITS_PER_LONG / 4,
		__entry->watching_ino,
		__entry->file_name,
		__print_flags(__entry->mask & FS_TRACED_EVENTS, "|",
			{ FS_ACCESS,		"ACCESS" },
			{ FS_MODIFY,		"MODIFY" },
			{ FS_ATTRIB,		"ATTRIB" },
			{ FS_CLOSE_WRITE,	"CLOSE_WRITE" },
			{ FS_CLOSE_NOWRITE,	"CLOSE_NOWRITE" },
			{ FS_OPEN,		"OPEN" },
			{ FS_MOVED_FROM,	"MOVED_FROM" },
			{ FS_MOVED_TO,		"MOVED_TO" },
			{ FS_CREATE,		"CREATE" },
			{ FS_DELETE,		"DELETE" },
			{ FS_DELETE_SELF,	"DELETE_SELF" },
			{ FS_MOVE_SELF,		"MOVE_SELF" },
			{ FS_OPEN_PERM,		"OPEN_PERM" },
			{ FS_ACCESS_PERM,	"ACCESS_PERM" }
		)
	)
);

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INLUCE_FILE
#define TRACE_INCLUDE_FILE trace-fsnotify

#endif /* _TRACE_FSNOTIFY_H */

#include <trace/define_trace.h>
