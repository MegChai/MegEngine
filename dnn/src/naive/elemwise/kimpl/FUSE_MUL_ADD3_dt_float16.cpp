// generated by gen_elemwise_kern_impls.py
#if !MEGDNN_DISABLE_FLOAT16
#define KERN_IMPL_MODE(cb) MEGDNN_ELEMWISE_MODE_ENABLE(FUSE_MUL_ADD3, cb)
#define KERN_IMPL_ARITY    3
#define KERN_IMPL_CTYPE    dt_float16
#include "../kern_impl.inl"
#endif
