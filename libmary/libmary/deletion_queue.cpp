/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/libmary_thread_local.h>
#include <libmary/debug.h>

#include <libmary/deletion_queue.h>


#define DEBUG(a)


namespace M {

/* Deletion queue is a singly-linked circular list of Objects. Link pointers
 * are stored in Object::atomic_shadow in non-atomic fashion. We store a pointer
 * to the last element in the queue in thread-local storage. This allows
 * us to append elements to the end of the queue efficiently.
 */

#ifdef LIBMARY_DELETION_QUEUE__PRINT_NUM_ENTRIES
namespace {
AtomicInt deletion_queue_num_entries;
}
#endif

void
deletionQueue_append (Object * const obj)
{
    DEBUG (
	static char const * const _func_name = "deletionQueue_append";
    )

    DEBUG (
	printf ("0x%lx %s\n", (unsigned long) obj, _func_name);
    )

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

#ifdef LIBMARY_DELETION_QUEUE__PRINT_NUM_ENTRIES
    fprintf (stderr, "deletionQueue_append: num_entries: %d\n", deletion_queue_num_entries.fetchAdd (1) + 1);
#endif

    Object * const last = tlocal->deletion_queue;
    if (last == NULL) {
	obj->atomic_shadow.set_nonatomic (static_cast <void*> (obj));
	tlocal->deletion_queue = obj;
	return;
    }

    obj->atomic_shadow.set_nonatomic (last->atomic_shadow.get_nonatomic ());
    last->atomic_shadow.set_nonatomic (static_cast <void*> (obj));
    tlocal->deletion_queue = obj;
}

void
deletionQueue_process ()
{
    DEBUG (
	static char const * const _func_name = "deletionQueue_process";
    )

    DEBUG (
	printf ("%s\n", _func_name);
    )

    // Note that deletionQueue_process() may be called recursively (from
    // destructors which lock and unlock 'StateMutex'es).
    //
    // We avoid recursive calls of deletionQueue_process() by setting a special
    // flag in thread-local storage. This is done to decrease stack pressure
    // and to simplify queue processing logics.

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    if (tlocal->deletion_queue_processing) {
      // Deletion queue processing is already in progress.
      // This is a nested invocation, which we do not want.

	DEBUG (
	    printf ("%s: deletion_queue_processing\n", _func_name);
	)

	return;
    }
    tlocal->deletion_queue_processing = true;

#ifdef LIBMARY_DELETION_QUEUE__PRINT_NUM_ENTRIES
    fprintf (stderr, "deletionQueue_process: num_entries: %d\n", deletion_queue_num_entries.get());
#endif

    for (;;) {
	Object * const last = tlocal->deletion_queue;
	if (last == NULL) {
	  // Deletion queue is empty.
	    break;
	}

	Object * const obj = static_cast <Object*> (last->atomic_shadow.get_nonatomic ());

	Object * const next_obj =
		static_cast <Object*> (obj->atomic_shadow.get_nonatomic ());

	if (next_obj == obj)
	    tlocal->deletion_queue = NULL;
	else
	    last->atomic_shadow.set_nonatomic (static_cast <void*> (next_obj));

	// We used obj->atomic_shadow as a linked list pointer, but Object
	// expects to be a Shadow pointer. Nullifying 'atomic_shadow' to avoid
	// confusion.
	obj->atomic_shadow.set_nonatomic (NULL);

	DEBUG (
	    printf ("0x%lx %s: deleting\n", (unsigned long) obj, _func_name);
	)
	obj->do_delete ();
#ifdef LIBMARY_DELETION_QUEUE__PRINT_NUM_ENTRIES
	fprintf (stderr, "deletionQueue_process: -obj, num_entries: %d\n", deletion_queue_num_entries.fetchAdd (-1) - 1);
#endif
    }

    tlocal->deletion_queue_processing = false;

    DEBUG (
	printf ("%s: done\n", _func_name);
    )
}

bool
deletionQueue_isEmpty ()
{
    return !libMary_getThreadLocal()->deletion_queue;
}

}

