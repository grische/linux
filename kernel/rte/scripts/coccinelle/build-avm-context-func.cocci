@is_avm_rte@
symbol is_avm_rte;
@@
(
- __BUILD_AVM_CONTEXT_FUNC(is_avm_rte)()
+ is_rte_context()
|
- __BUILD_AVM_CONTEXT_FUNC(is_avm_rte())
+ is_rte_context()
|
- _BUILD_AVM_CONTEXT_FUNC(is_avm_rte)()
+ is_rte_context()
|
- _BUILD_AVM_CONTEXT_FUNC(is_avm_rte())
+ is_rte_context()
)

@rte_synchronize_timeout@
symbol to_linux_sync_ipi;
expression timeout;
@@
(
- __BUILD_AVM_CONTEXT_FUNC(to_linux_sync_ipi)(timeout)
+ rte_synchronize_timeout(timeout)
|
- __BUILD_AVM_CONTEXT_FUNC(to_linux_sync_ipi(timeout))
+ rte_synchronize_timeout(timeout)
|
- _BUILD_AVM_CONTEXT_FUNC(to_linux_sync_ipi)(timeout)
+ rte_synchronize_timeout(timeout)
|
- _BUILD_AVM_CONTEXT_FUNC(to_linux_sync_ipi(timeout))
+ rte_synchronize_timeout(timeout)
)

@build_avm_context_func@
identifier func;
expression list args;
fresh identifier rte_func = "rte_"## func;
@@
(
- __BUILD_AVM_CONTEXT_FUNC(func)(args)
+ rte_func(args)
|
- __BUILD_AVM_CONTEXT_FUNC(func(args))
+ rte_func(args)
|
- _BUILD_AVM_CONTEXT_FUNC(func)(args)
+ rte_func(args)
|
- _BUILD_AVM_CONTEXT_FUNC(func(args))
+ rte_func(args)
)
