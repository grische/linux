@spin_lock_init@
expression lock;
@@
- pcmlink_ul_lock_init(lock)
+ rte_spin_lock_init(lock)

@spin_lock@
expression lock;
@@
- pcmlink_ul_lock(lock, NULL)
+ rte_spin_lock(lock)

@spin_lock_irqsave@
expression lock, flags;
expression *pflags;
@@
(
- pcmlink_ul_lock(lock, &flags)
+ rte_spin_lock_irqsave(lock, flags)
|
- pcmlink_ul_lock(lock, pflags)
+ rte_spin_lock_irqsave(lock, *pflags)
)

@spin_unlock@
expression lock;
@@
- pcmlink_ul_unlock(lock, NULL)
+ rte_spin_unlock(lock)

@spin_unlock_irqrestore@
expression lock, flags;
expression *pflags;
@@
(
- pcmlink_ul_unlock(lock, &flags)
+ rte_spin_unlock_irqrestore(lock, flags)
|
- pcmlink_ul_unlock(lock, pflags)
+ rte_spin_unlock_irqrestore(lock, *pflags)
)

@is_rte_context@
@@
- pcmlink_ul_yieldcontext()
+ is_rte_context()

/* Remove RTE context assertions in void context. */
@remove_simple_rte_assert@
@@
  ...
- pcmlink_ul_assert_on_yield_context();

@remove_rte_assert@
declaration decl;
statement St;
@@
(
-   if (pcmlink_ul_assert_on_yield_context() != 0)
-       St
|
-   if (pcmlink_ul_assert_on_yield_context() == 0) {
        ... when != decl
-   }
|
-   if (pcmlink_ul_assert_on_yield_context() == 0)
        St
)

/*
 * Replace pcmlink_ul_assert_on_yield_context() in non-void context.
 * We strip the warning and only preserve the test, since the code should be
 * well tested now.  (Fingers crossed.)
 */
@assert_not_rte@
@@
- pcmlink_ul_assert_on_yield_context()
+ is_rte_context()

@wake_up_interruptible@
expression q;
@@
- pcmlink_ul_wake_up_interruptible(q)
+ rte_wake_up_interruptible(q)

@wake_up@
expression q;
@@
- pcmlink_ul_wake_up(q)
+ rte_wake_up(q)

@schedule_work@
expression work;
@@
- pcmlink_ul_schedule_work(work)
+ rte_schedule_work(work)

@queue_work_on@
expression cpu, wq, work;
@@
- pcmlink_ul_queue_work_on(cpu, wq, work)
+ rte_queue_work_on(cpu, wq, work)

@schedule_delayed_work@
expression dwork, delay;
@@
- pcmlink_ul_schedule_delayed_work(dwork, delay)
+ rte_schedule_delayed_work(dwork, delay)

@tasklet_hi_schedule@
expression t;
@@
- pcmlink_ul_tasklet_hi_schedule(t)
+ rte_tasklet_hi_schedule(t)

@try_module_get@
expression module;
@@
- pcmlink_ul_try_module_get(module)
+ rte_try_module_get(module)

@module_put@
expression module;
@@
- pcmlink_ul_module_put(module)
+ rte_module_put(module)

@rte_synchronize_timeout@
expression timeout;
@@
- pcmlink_ul_to_linux_sync_ipi(timeout)
+ rte_synchronize_timeout(timeout)

@rte_mempool_create_chk@
identifier ph;
expression name, size;
statement on_error;
@@
(
- ph = pcmlink_ul_alloc_init(name, size);
- if (ph == NULL)
+ ph = rte_mempool_create(name, size);
+ if (IS_ERR(ph))
  {
+   ph = NULL;
    ...
  }
|
- ph = pcmlink_ul_alloc_init(name, size);
- if (ph == NULL)
+ ph = rte_mempool_create(name, size);
+ if (IS_ERR(ph)) {
+    ph = NULL;
     on_error
+ }
)

@rte_mempool_create_nochk@
identifier ph;
expression name, size;
@@
- ph = pcmlink_ul_alloc_init(name, size);
+ ph = rte_mempool_create(name, size);

@rte_mempool_destroy@
expression ph;
@@
- pcmlink_ul_alloc_exit(ph)
+ rte_mempool_destroy(ph)

@rte_kmalloc@
expression ph, size;
@@
- pcmlink_ul_malloc(ph, size)
+ rte_kmalloc(ph, size)

@rte_kzalloc@
expression ph, size;
@@
- pcmlink_ul_zalloc(ph, size)
+ rte_kzalloc(ph, size)

@rte_kfree@
expression ph, ptr;
@@
- pcmlink_ul_mfree(ph, ptr)
+ rte_kfree(ph, ptr)
