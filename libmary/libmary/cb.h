/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CB__H__
#define LIBMARY__CB__H__


#include <libmary/types.h>
#include <libmary/virt_ref.h>
#include <libmary/libmary_thread_local.h>


#ifdef DEBUG
  #error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
  #include <cstdio>
#endif


namespace M {

class DeferredProcessor_Registration;

// TODO For each call(), provide the calling Object as the first parameter
//      and implement synchronization shortcuts (no need to do getRef() when
//      called from the same coderef container).
template <class T>
class Cb
{
private:
    // Frontend/backend.
    T const         *cb;
    void            *cb_data;
    WeakRef<Object>  weak_ref;
    VirtRef          ref_data;

public:
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class RET, class CB, class ...Args>
    bool call_ret (RET * const mt_nonnull ret, CB tocall, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: callback not set, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )
	    return false;
	}

	if (weak_ref.isValid ()) {
	  // The weak reference is valid, which means that we should grab a real
	  // code reference first.

	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow) {
	      // The container has already been code-referenced in the current
	      // execution path. There's no need to duplicate the reference.
	      //
	      // In this case, it must be possible to prove that we're not
	      // comparing a pointer to an old object to a pointer to a new
	      // object located at the same address. This is true for conatiner
	      // logics in MomentVideo.
		goto _simple_path;
	    }

	    Ref<Object> code_ref = weak_ref.getRef ();
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call_ret: obj 0x%lx gone\n", (unsigned long) weak_ref.getRefPtr());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: refed obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )

	    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr();

	    // This probably won't be optimized by the compiler, which is a bit sad.
	    *ret = tocall (args..., cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            // It is important to nullify *after* restoring last_coderef_container_shadow,
            // because dtor may call WeakRef::getRef() on code_ref'ed object.
            // This applies to other similar places as well.
            code_ref = NULL;
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call_ret: simple path, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
	*ret = tocall (args..., cb_data);
	DEBUG (
	  fprintf (stderr, "Cb::call_ret: done, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
	return true;
    }

    // Convenient mutex-aware method of invoking callbacks with a return value.
    //
    // Before the call, unlocks the mutex given. After the call is complete,
    // locks the mutex.
    //
    // The object that cb is pointing to and this Cb object may be destroyed
    // safely after the mutex is unlocked.
    //
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class RET, class CB, class MutexType, class ...Args>
    bool call_ret_mutex (RET * const mt_nonnull ret, CB tocall, MutexType &mutex, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret_mutex: callback not set, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )
	    return false;
	}

	if (weak_ref.isValid ()) {
	  // The weak reference is valid, which means that we should grab a real
	  // code reference first.

	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow) {
	      // The container has already been code-referenced in the current
	      // execution path. There's no need to duplicate the reference.
	      //
	      // In this case, it must be possible to prove that we're not
	      // comparing a pointer to an old object to a pointer to a new
	      // object located at the same address. This is true for conatiner
	      // logics in MomentVideo.
		goto _simple_path;
	    }

	    Ref<Object> code_ref = weak_ref.getRef();
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call_ret_mutex: obj 0x%lx gone\n", (unsigned long) weak_ref.getRefPtr());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret_mutex: refed obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )

	    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr ();

            // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
            // which is not guaranteed to stay available after we unlock the mutex.
            void * const tmp_cb_data = cb_data;
	    mutex.unlock ();

	    // Be careful not to use any data members of class Cb after the mutex is unlocked.
            //
	    // This probably won't be optimized by the compiler, which is a bit sad.
	    *ret = tocall (args..., tmp_cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            code_ref = NULL;
	    mutex.lock ();
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret_mutex: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call_ret_mutex: simple path, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
        // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
        // which is not guaranteed to stay available after we unlock the mutex.
        void * const tmp_cb_data = cb_data;
	mutex.unlock ();

	// Be careful not to use any data members of class Cb after the mutex is unlocked.
	*ret = tocall (args..., tmp_cb_data);

	mutex.lock ();
	DEBUG (
	  fprintf (stderr, "Cb::call_ret_mutex: done, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
	return true;
    }

    template <class CB, class ...Args>
    void call_deferred (DeferredProcessor_Registration * mt_nonnull def_reg,
                        CB                             *tocall,
                        VirtReferenced                 *extra_ref_data,
                        Args                            ...args) const;

    // Convenient method of invoking callbacks which return void.
    //
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class CB, class ...Args>
    bool call (CB tocall, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call: callback not set, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )
	    return false;
	}

	if (weak_ref.isValid ()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow)
		goto _simple_path;

	    Ref<Object> code_ref = weak_ref.getRef ();
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call: obj 0x%lx gone\n", (unsigned long) weak_ref.getRefPtr());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call: refed obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )

	    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr();

	    tocall (args..., cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            code_ref = NULL;
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call: simple path, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
	tocall (args..., cb_data);
	DEBUG (
	  fprintf (stderr, "Cb::call: done, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
	return true;
    }

    // Convenient mutex-aware method of invoking callbacks which return void.
    //
    // Before the call, unlocks the mutex given. After the call is complete,
    // locks the mutex.
    //
    // The object that cb is pointing to and this Cb object may be destroyed
    // safely after the mutex is unlocked.
    //
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class CB, class MutexType, class ...Args>
    bool call_mutex (CB tocall, MutexType &mutex, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call_mutex: callback not set, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )
	    return false;
	}

	if (weak_ref.isValid ()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow)
		goto _simple_path;

	    Ref<Object> code_ref = weak_ref.getRef ();
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call_mutex: obj 0x%lx gone\n", (unsigned long) weak_ref.getRefPtr());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call_mutex: refed obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )

	    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr ();

            // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
            // which is not guaranteed to stay available after we unlock the mutex.
            void * const tmp_cb_data = cb_data;
	    mutex.unlock ();

	    // Be careful not to use any data members of class Cb after the mutex is unlocked.
	    tocall (args..., tmp_cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            code_ref = NULL;
	    mutex.lock ();
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call_mutex: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call_mutex: simple path, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
        // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
        // which is not guaranteed to stay available after we unlock the mutex.
        void * const tmp_cb_data = cb_data;
	mutex.unlock ();

	// Be careful not to use any data members of class Cb after the mutex is unlocked.
	tocall (args..., tmp_cb_data);

	mutex.lock ();
	DEBUG (
	  fprintf (stderr, "Cb::call_mutex: done\n");
	)
	return true;
    }

    template <class CB, class MutexType, class ...Args>
    bool call_unlocks_mutex (CB tocall, MutexType &mutex, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call_unlocks_mutex: callback not set, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )
	    mutex.unlock ();
	    return false;
	}

	if (weak_ref.isValid ()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow)
		goto _simple_path;

	    Ref<Object> code_ref = weak_ref.getRef ();
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call_unlocks_mutex: obj 0x%lx gone\n", (unsigned long) weak_ref.getRefPtr());
		)
		mutex.unlock ();
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call_unlocks_mutex: refed obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	    )

	    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr();

            // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
            // which is not guaranteed to stay available after we unlock the mutex.
            void * const tmp_cb_data = cb_data;
	    mutex.unlock ();

	    // Be careful not to use any data members of class Cb after the mutex is unlocked.
	    tocall (args..., tmp_cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            code_ref = NULL;
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call_unlocks_mutex: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call_unlocks_mutex: simple path, obj 0x%lx\n", (unsigned long) weak_ref.getRefPtr());
	)
        // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
        // which is not guaranteed to stay available after we unlock the mutex.
        void * const tmp_cb_data = cb_data;
	mutex.unlock ();

	// Be careful not to use any data members of class Cb after the mutex is unlocked.
	tocall (args..., tmp_cb_data);

	DEBUG (
	  fprintf (stderr, "Cb::call_unlocks_mutex: done\n");
	)
	return true;
    }

    // Doesn't unlock the mutex when 'false' is returned.
    template <class CB, class MutexType, class ...Args>
    bool call_unlocks_mutex_if_called (CB tocall, MutexType &mutex, Args const &...args) const
    {
	if (!tocall)
	    return false;

	if (weak_ref.isValid ()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (weak_ref.getShadowPtr() == tlocal->last_coderef_container_shadow)
		goto _simple_path;

	    Ref<Object> code_ref = weak_ref.getRef ();
	    if (!code_ref)
		return false;

            Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
	    tlocal->last_coderef_container_shadow = weak_ref.getShadowPtr();

            // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
            // which is not guaranteed to stay available after we unlock the mutex.
            void * const tmp_cb_data = cb_data;
	    mutex.unlock ();

	    // Be careful not to use any data members of class Cb after the mutex is unlocked.
	    tocall (args..., tmp_cb_data);

	    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;
            code_ref = NULL;
	    return true;
	}

      _simple_path:
        // With 'tmp_cb_data', we avoid accessing 'cb_data', since it is a member of Cb,
        // which is not guaranteed to stay available after we unlock the mutex.
        void * const tmp_cb_data = cb_data;
	mutex.unlock ();

	// Be careful not to use any data members of class Cb after the mutex is unlocked.
	tocall (args..., tmp_cb_data);

	return true;
    }

    template <class RET, class ...Args>
    bool call_ret_ (RET * const mt_nonnull ret, Args const &...args) const
        { return call_ret (ret, cb, args...); }

    template <class ...Args>
    bool call_ (Args const &...args) const
        { return call (cb, args...); }

    template <class MutexType, class RET, class ...Args>
    bool call_ret_mutex_ (MutexType &mutex, RET * const mt_nonnull ret, Args const &...args) const
        { return call_ret_mutex (ret, cb, mutex, args...); }

    template <class MutexType, class ...Args>
    bool call_mutex_ (MutexType &mutex, Args const &...args) const
        { return call_mutex (cb, mutex, args...); }

    template <class MutexType, class ...Args>
    bool call_unlocks_mutex_ (MutexType &mutex, Args const &...args) const
        { return call_unlocks_mutex (cb, mutex, args...); }

    template <class MutexType, class ...Args>
    bool call_unlocks_mutex_if_called_ (MutexType &mutex, Args const &...args) const
        { return call_unlocks_mutex_if_called (cb, mutex, args...); }

#if 0
// Commented out for safe transition to Cb::call()/Cb::call_ret().
    void* data () const
    {
	return cb_data;
    }
#endif

    WeakRef<Object> const & getWeakRef () const { return weak_ref; }

    void* getCbData () const { return cb_data; }

    operator T const * () const    { return cb; }
    T const * operator -> () const { return cb; }

    Cb& operator = (CbDesc<T> const &cb_desc)
    {
	cb       = cb_desc.cb;
	cb_data  = cb_desc.cb_data;
	weak_ref = cb_desc.guard_obj;
        ref_data = cb_desc.ref_data;

	return *this;
    }

    void reset ()
    {
	cb = NULL;
	cb_data = NULL;
	weak_ref = NULL;
	ref_data = NULL;
    }

    // TODO After introduction of coderef containers, copying Cb<>'s around
    //      became more expensive because of Ref<Shadow> member. Figure out
    //      what's the most effective way to avoid excessive atomic ref/unref
    //      operations.
    Cb (T const        * const cb,
	void           * const cb_data,
	Object         * const guard_obj,
	VirtReferenced * const ref_data = NULL)
	: cb       (cb),
	  cb_data  (cb_data),
	  weak_ref (guard_obj),
	  ref_data (ref_data)
    {}

    Cb (T const               * const cb,
        void                  * const cb_data,
        WeakRef<Object> const * const weak_ref,
        VirtReferenced        * const ref_data)
        : cb       (cb),
          cb_data  (cb_data),
          weak_ref (*weak_ref),
          ref_data (ref_data)
    {}

    Cb (CbDesc<T> const &cb_desc)
	: cb       (cb_desc.cb),
	  cb_data  (cb_desc.cb_data),
	  weak_ref (cb_desc.guard_obj),
	  ref_data (cb_desc.ref_data)
    {}

    Cb ()
	: cb (NULL),
	  cb_data (NULL)
    {}
};

}


#ifdef DEBUG
#undef DEBUG
#endif


#include <libmary/deferred_processor.h> // for cb_deferred.h


#endif /* LIBMARY__CB__H__ */

