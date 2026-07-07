// Reproducer for Project-HAMi/HAMi-core issue #93 (the same untracked-free leak
// class reported in #67): cuMemAllocFromPoolAsync allocations are invisible to
// libvgpu, so the matching cuMemFreeAsync is rejected.
//
// Mechanism (src/cuda/memory.c + src/allocator/allocator.c on main):
//   cuMemFreeAsync() -> free_raw_async() -> remove_chunk_async() walks the
//   device_allocasync list for the pointer. cuMemAllocAsync() registers there
//   (via add_chunk_async); cuMemAllocFromPoolAsync() is a bare pass-through and
//   never does. For a pool-explicit allocation the pointer is not found, so
//   remove_chunk_async() returns -1 WITHOUT calling the real cuMemFreeAsync:
//     (a) -1 surfaces as a CUresult -> "unrecognized error code", and
//     (b) the real free is skipped -> the device allocation leaks.
//
// Path [A] (default pool) is the control: it IS tracked, so it still frees
// cleanly under libvgpu. Path [B] (explicit pool) is the bug. On stock CUDA
// (no LD_PRELOAD) both paths return CUDA_SUCCESS.
//
// Build (standalone, no HAMi headers needed):
//   gcc test_alloc_pool_async.c -o test_alloc_pool_async \
//       -I/usr/local/cuda/include -L/usr/local/cuda/lib64/stubs -lcuda
// Or drop into HAMi-core/test/ and `make build-in-docker` (auto-discovered).
//
// Run baseline (no libvgpu, expect PASS):
//   ./test_alloc_pool_async
// Run under libvgpu (expect FAIL until fixed):
//   LD_PRELOAD=/path/to/libvgpu.so CUDA_DEVICE_MEMORY_LIMIT=2g \
//     LIBCUDA_LOG_LEVEL=1 ./test_alloc_pool_async

#include <cuda.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST_DEVICE_ID
#define TEST_DEVICE_ID 0
#endif

#define ALLOC_BYTES (32ull * 1024 * 1024) /* 32 MiB */

/* Abort-on-error for setup calls that must succeed for the test to be valid. */
#define CHECK_DRV(call)                                                        \
    do {                                                                       \
        CUresult _e = (call);                                                  \
        if (_e != CUDA_SUCCESS) {                                              \
            const char *_n = NULL;                                             \
            cuGetErrorName(_e, &_n);                                           \
            fprintf(stderr, "FATAL %s:%d: %s -> %d (%s)\n", __FILE__,          \
                    __LINE__, #call, (int)_e, _n ? _n : "?");                  \
            return 2;                                                          \
        }                                                                      \
    } while (0)

/* Print a CUresult, flagging codes the driver itself does not recognise
   (exactly what a caller sees as "unrecognized error code"). */
static void describe(const char *label, CUresult res) {
    const char *name = NULL;
    CUresult q = cuGetErrorName(res, &name);
    if (q != CUDA_SUCCESS || name == NULL)
        printf("  %-34s -> %d  <unrecognized error code>\n", label, (int)res);
    else
        printf("  %-34s -> %d  (%s)\n", label, (int)res, name);
}

int main(void) {
    CHECK_DRV(cuInit(0));

    CUdevice dev;
    CHECK_DRV(cuDeviceGet(&dev, TEST_DEVICE_ID));

    CUcontext ctx;
#if CUDA_VERSION >= 13000
    CHECK_DRV(cuCtxCreate(&ctx, NULL, 0, dev));
#else
    CHECK_DRV(cuCtxCreate(&ctx, 0, dev));
#endif

    CUstream stream;
    CHECK_DRV(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));

    int failures = 0;

    /* [A] Control: default-pool async path. libvgpu tracks this one. */
    printf("[A] cuMemAllocAsync + cuMemFreeAsync (default pool)\n");
    {
        CUdeviceptr d = 0;
        CHECK_DRV(cuMemAllocAsync(&d, ALLOC_BYTES, stream));
        CUresult f = cuMemFreeAsync(d, stream);
        describe("cuMemFreeAsync", f);
        CHECK_DRV(cuStreamSynchronize(stream));
        if (f != CUDA_SUCCESS)
            failures++;
    }

    /* [B] Bug: explicit-pool async path. libvgpu never tracks this one. */
    printf("[B] cuMemAllocFromPoolAsync + cuMemFreeAsync (explicit pool)\n");
    {
        CUmemPoolProps props;
        memset(&props, 0, sizeof(props));
        props.allocType = CU_MEM_ALLOCATION_TYPE_PINNED;
        props.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
        props.location.id = dev;

        CUmemoryPool pool;
        CHECK_DRV(cuMemPoolCreate(&pool, &props));

        CUdeviceptr d = 0;
        CHECK_DRV(cuMemAllocFromPoolAsync(&d, ALLOC_BYTES, pool, stream));
        CUresult f = cuMemFreeAsync(d, stream);
        describe("cuMemFreeAsync", f);
        if (f != CUDA_SUCCESS)
            failures++;

        /* Best-effort teardown; a rejected free leaves the allocation live. */
        cuStreamSynchronize(stream);
        cuMemPoolDestroy(pool);
    }

    printf("\nResult: %s\n",
           failures ? "FAIL - pool-async free rejected (issue #93 reproduced)"
                    : "PASS - both async free paths accepted");

    cuStreamDestroy(stream);
    cuCtxDestroy(ctx);
    return failures ? 1 : 0;
}
