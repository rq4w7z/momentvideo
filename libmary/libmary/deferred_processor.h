/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__DEFERRED_PROCESSOR__H__
#define LIBMARY__DEFERRED_PROCESSOR__H__


#include <libmary/types.h>
#include <libmary/cb.h>
#include <libmary/intrusive_list.h>
#include <libmary/state_mutex.h>


namespace M {

// DeferredProcessor::Task and DeferredProcessor::Registration used to be nested classes.
// They were moved to namespace M to allow forward declaration of DeferredProcessor_Registration,
// which is needed to declare Cb<>::call_deferred().

// Returns 'true' if the task should be rescheduled immediately for another
// invocation.
typedef bool DeferredProcessor_TaskCallback (void *cb_data);

class DeferredProcessor;
class DeferredProcessor_Registration;

class DeferredProcessor_TaskList_name;
class DeferredProcessor_PermanentTaskList_name;

class DeferredProcessor_Task : public IntrusiveListElement<DeferredProcessor_TaskList_name>,
                               public IntrusiveListElement<DeferredProcessor_PermanentTaskList_name>
{
    friend class DeferredProcessor;
    friend class DeferredProcessor_Registration;

private:
    mt_mutex (DeferredProcessor::mutex) bool scheduled;
    mt_mutex (DeferredProcessor::mutex) bool processing;
    mt_mutex (DeferredProcessor::mutex) bool permanent;

    mt_mutex (DeferredProcessor::mutex) DeferredProcessor_Registration *registration;

public:
    mt_const Cb<DeferredProcessor_TaskCallback> cb;
    mt_const VirtRef self_ref;

    DeferredProcessor_Task ()
        : scheduled    (false),
          processing   (false),
          permanent    (false),
          registration (NULL)
    {}
};

typedef IntrusiveList <DeferredProcessor_Task,
                       DeferredProcessor_TaskList_name> DeferredProcessor_TaskList;

typedef IntrusiveList <DeferredProcessor_Task,
                       DeferredProcessor_PermanentTaskList_name> DeferredProcessor_PermanentTaskList;

class DeferredProcessor_RegistrationList_name;
class DeferredProcessor_PermanentRegistrationList_name;

class DeferredProcessor_Registration
        : public IntrusiveListElement<DeferredProcessor_RegistrationList_name>,
          public IntrusiveListElement<DeferredProcessor_PermanentRegistrationList_name>
{
    friend class DeferredProcessor;

private:
    mt_const WeakRef<DeferredProcessor> weak_deferred_processor;

    mt_mutex (deferred_processor->mutex) DeferredProcessor_TaskList task_list;
    mt_mutex (deferred_processor->mutex) DeferredProcessor_PermanentTaskList permanent_task_list;

    mt_mutex (deferred_processor->mutex) bool scheduled;
    mt_mutex (deferred_processor->mutex) bool permanent_scheduled;

    mt_mutex (deferred_processor->mutex) void rescheduleTask (DeferredProcessor_Task * mt_nonnull task);

public:
    bool isValid () const { return weak_deferred_processor.isValid(); }

    void scheduleTask (DeferredProcessor_Task * mt_nonnull task,
                       bool                    permanent = false);

    void revokeTask (DeferredProcessor_Task * mt_nonnull task);

    mt_const void setDeferredProcessor (DeferredProcessor * const mt_nonnull deferred_processor)
        { this->weak_deferred_processor = deferred_processor; }

    void release ();

    DeferredProcessor_Registration ()
        : scheduled (false),
          permanent_scheduled (false)
    {}

    ~DeferredProcessor_Registration ()
        { release (); }
};

typedef IntrusiveList <DeferredProcessor_Registration,
                       DeferredProcessor_RegistrationList_name>
        DeferredProcessor_RegistrationList;

typedef IntrusiveList <DeferredProcessor_Registration,
                       DeferredProcessor_PermanentRegistrationList_name>
        DeferredProcessor_PermanentRegistrationList;

class DeferredProcessor : public Object
{
private:
    // This could be a plain Mutex, but there was a synchronization bug in Cb::call_mutex,
    // so this is extra-safe now.
    StateMutex mutex;

public:
    typedef DeferredProcessor_TaskCallback TaskCallback;

    struct Backend
    {
	void (*trigger) (void *cb_data);
    };

    friend class DeferredProcessor_Registration;

    typedef DeferredProcessor_Task              Task;
    typedef DeferredProcessor_TaskList          TaskList;
    typedef DeferredProcessor_PermanentTaskList PermanentTaskList;

    typedef DeferredProcessor_Registration              Registration;
    typedef DeferredProcessor_RegistrationList          RegistrationList;
    typedef DeferredProcessor_PermanentRegistrationList PermanentRegistrationList;

private:
    Cb<Backend> backend;

    mt_mutex (mutex) RegistrationList registration_list;
    mt_mutex (mutex) PermanentRegistrationList permanent_registration_list;

    mt_mutex (mutex) bool processing;
    mt_mutex (mutex) TaskList processing_task_list;

public:
    // Returns 'true' if there are more tasks to process.
    bool process ();

    // Schedules processing of permanent tasks.
    void trigger ();

    mt_const void setBackend (CbDesc<Backend> const &backend)
        { this->backend = backend; }

    DeferredProcessor (EmbedContainer *embed_container);
};

}


#include <libmary/cb_deferred.h>


#endif /* LIBMARY__DEFERRED_PROCESSOR__H__ */

