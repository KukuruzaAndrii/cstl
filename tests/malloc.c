#define FIO_CORE
#define FIO_CLI
#define FIO_THREADS
#include <fio-stl.h>
/* use the fast-setup global allocator shortcut for FIO_MEMORY_NAME */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 0
#define FIO_MALLOC
#ifdef DEBUG
/*
 * when debugging, use less arenas, it makes it faster to track contention
 * related issues
 */
#define FIO_MEMORY_ARENA_COUNT 2
#endif
#include FIO_INCLUDE_FILE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if !FIO_OS_WIN
#include <sys/resource.h>
#endif

static size_t TEST_CYCLES_START;
static size_t TEST_CYCLES_END;
static size_t TEST_CYCLES_REPEAT;

/**
 * facil.io doesn't keep metadata in memory slices, panelizing realloc.
 *
 * To compensate, it provides fio_realloc2 that copies only the valid data.
 *
 * If this flag is set, the test will run using this compensation, assuming all
 * previously allocated memory should be copied (but no risk of copy overflow
 * performance hit).
 */
#define TEST_WITH_REALLOC2 1

/**
 * Tests interleaved allocation-free pattern
 */
#define TEST_WITH_INTERLEAVED_FREE 1

/* *****************************************************************************
Testing implementation
***************************************************************************** */

#ifndef FIO_MALLOC_TEST_NOTICE
#define FIO_MALLOC_TEST_NOTICE                                                 \
  "Testing FIO_MEMORY_NAME with global allocator settings (FIO_MALLOC)."
#endif

#if TEST_WITH_REALLOC2
FIO_SFUNC void *sys_realloc2(void *ptr, size_t new_size, size_t copy_len) {
  return realloc(ptr, new_size);
  (void)copy_len;
}
typedef void *(*realloc_func_p)(void *, size_t, size_t);
#define REALLOC_FUNC(func, ptr, new_size, copy_len)                            \
  func(ptr, new_size, copy_len)
#else
typedef void *(*realloc_func_p)(void *, size_t);
#define REALLOC_FUNC(func, ptr, new_size, copy_len) func(ptr, new_size)
#endif

static size_t test_mem_functions(void *(*malloc_func)(size_t),
                                 void *(*calloc_func)(size_t, size_t),
                                 realloc_func_p realloc_func,
                                 void (*free_func)(void *)) {
  static size_t clock_alloc = 0, clock_realloc = 0, clock_free = 0,
                clock_free2 = 0, clock_calloc = 0, fio_optimized = 0,
                fio_optimized2 = 0, errors = 0, repetitions = 0;

  const size_t size_units = TEST_CYCLES_END - TEST_CYCLES_START;
  const size_t pointers_per_unit =
      ((size_units >= 4096) ? 1 : (4096 / size_units));
  const size_t total_pointers = pointers_per_unit * size_units;

  if (!malloc_func) {
    size_t total = clock_alloc + clock_realloc + clock_free + clock_free2 +
                   clock_calloc + fio_optimized + fio_optimized2;
    clock_alloc /= repetitions;
    clock_realloc /= repetitions;
    clock_free /= repetitions;
    clock_free2 /= repetitions;
    clock_calloc /= repetitions;
    fio_optimized /= repetitions;
    fio_optimized2 /= repetitions;
    if (!calloc_func) {
      FIO_LOG_DEBUG2("* size_units: %zu, ppu: %zu, total pointers: %zu \n",
                     size_units,
                     pointers_per_unit,
                     total_pointers);
      fprintf(stderr, "* Micro-seconds performing malloc: %zu\n", clock_alloc);
      fprintf(stderr, "* Micro-seconds performing calloc: %zu\n", clock_calloc);
      fprintf(stderr,
              "* Micro-seconds performing realloc: %zu\n",
              clock_realloc);
      fprintf(stderr,
              "* Micro-seconds performing free (realloc): %zu\n",
              clock_free);
      fprintf(stderr,
              "* Micro-seconds performing free (re-cycle): %zu\n",
              clock_free2);
      fprintf(stderr,
              "* Micro-seconds performing a zero-life span"
              " (malloc-free): %zu\n",
              fio_optimized);
#if TEST_WITH_INTERLEAVED_FREE
      fprintf(stderr,
              "* Micro-seconds performing a facil.io use-case round"
              " (medium-short life): %zu\n",
              fio_optimized2);
#endif
      fprintf(stderr, "* Failed allocations: %zu\n", errors);
      fprintf(stderr, "Total CPU Time (micros): %zu\n", total);
    }
    clock_alloc = 0;
    clock_realloc = 0;
    clock_free = 0;
    clock_free2 = 0;
    clock_calloc = 0;
    fio_optimized = 0;
    fio_optimized2 = 0;
    errors = 0;
    repetitions = 0;
    return 0;
  }

  void **pointers = fio_mmap(sizeof(*pointers) * (total_pointers + 1));

  fio_atomic_add(&repetitions, TEST_CYCLES_REPEAT);
  for (size_t repeat = 0; repeat < TEST_CYCLES_REPEAT; ++repeat) {
    uint64_t start;
    /* malloc */
    start = fio_time_micro();
    for (size_t unit = 0; unit < size_units; ++unit) {
      const size_t bytes = (TEST_CYCLES_START + unit) << 4;
      for (size_t pi = 0; pi < pointers_per_unit; ++pi) {
        const size_t i = ((pointers_per_unit * unit) + pi);
        pointers[i] = malloc_func(bytes);
        if (bytes) {
          if (!pointers[i])
            ++errors;
          else
            ((char *)pointers[i])[0] = '1';
        }
      }
    }
    fio_atomic_add(&clock_alloc, fio_time_micro() - start);

    /* realloc */
    start = fio_time_micro();
    for (size_t unit = 0; unit < size_units; ++unit) {
      const size_t bytes = (TEST_CYCLES_START + unit) << 4;
      for (size_t pi = 0; pi < pointers_per_unit; ++pi) {
        const size_t i = (pointers_per_unit * unit) + pi;
        void *tmp =
            REALLOC_FUNC(realloc_func, pointers[i], (bytes << 1), bytes);
        if (bytes) {
          if (tmp) {
            pointers[i] = tmp;
          } else
            ++errors;
        }
      }
    }
    fio_atomic_add(&clock_realloc, fio_time_micro() - start);

    /* free (bigger sizes, due to realloc) */
    start = fio_time_micro();
    for (size_t i = 0; i < total_pointers; ++i) {
      free_func(pointers[i]);
      pointers[i] = NULL;
    }
    fio_atomic_add(&clock_free, fio_time_micro() - start);

    /* calloc */
    start = fio_time_micro();
    for (size_t unit = 0; unit < size_units; ++unit) {
      const size_t bytes = (TEST_CYCLES_START + unit) << 4;
      for (size_t pi = 0; pi < pointers_per_unit; ++pi) {
        const size_t i = (pointers_per_unit * unit) + pi;
        pointers[i] = calloc_func(bytes, 1);
        if (bytes) {
          if (!pointers[i])
            ++errors;
          else
            ((char *)pointers[i])[0] = '1';
        }
      }
    }
    fio_atomic_add(&clock_calloc, fio_time_micro() - start);

    /* free (smaller sizes, no realloc) */
    start = fio_time_micro();
    for (size_t i = 0; i < total_pointers; ++i) {
      free_func(pointers[i]);
      pointers[i] = NULL;
    }
    fio_atomic_add(&clock_free2, fio_time_micro() - start);

    /* immediate use-release */
    start = fio_time_micro();
    for (size_t pi = 0; pi < pointers_per_unit; ++pi) {
      for (size_t unit = 0; unit < size_units; ++unit) {
        const size_t bytes = (TEST_CYCLES_START + unit) << 4;
        const size_t i = (pointers_per_unit * unit) + pi;
        pointers[i] = malloc_func(bytes);
        if (bytes) {
          if (!pointers[i])
            ++errors;
          else
            ((char *)pointers[i])[0] = '1';
        }
        free_func(pointers[i]);
        pointers[i] = NULL;
      }
    }
    fio_atomic_add(&fio_optimized, fio_time_micro() - start);

    /* facil.io use-case - keep a while and free */
    for (size_t i = 0; i < 64; ++i) {
      pointers[i] = NULL;
    }
    start = fio_time_micro();
#if TEST_WITH_INTERLEAVED_FREE
    for (size_t unit = 0; unit < size_units; ++unit) {
      const size_t bytes = (TEST_CYCLES_START + unit) << 4;
      for (size_t i = 0; i < 32; ++i) {
        pointers[i + 32] = pointers[i];
        pointers[i] = malloc_func(bytes);
        if (bytes) {
          if (!pointers[i])
            ++errors;
          else
            ((char *)pointers[i])[0] = '1';
        }
      }
      for (size_t i = 0; i < 32; ++i) {
        free_func(pointers[i + 32]);
        pointers[i + 32] = NULL;
      }
    }
    for (size_t i = 0; i < 64; ++i) {
      free_func(pointers[i]);
      pointers[i] = NULL;
    }
#endif
    fio_atomic_add(&fio_optimized2, fio_time_micro() - start);
  }
  fio_free(pointers);
  return clock_alloc + clock_realloc + clock_free + clock_calloc + clock_free2;
}

void *test_system_malloc(void *ignr) {
  (void)ignr;
#if TEST_WITH_REALLOC2
  uintptr_t result = test_mem_functions(malloc, calloc, sys_realloc2, free);
#else
  uintptr_t result = test_mem_functions(malloc, calloc, realloc, free);
#endif
  return (void *)result;
}
void *test_facil_malloc(void *ignr) {
  (void)ignr;
#if TEST_WITH_REALLOC2
  uintptr_t result =
      test_mem_functions(fio_malloc, fio_calloc, fio_realloc2, fio_free);
#else
  uintptr_t result =
      test_mem_functions(fio_malloc, fio_calloc, fio_realloc, fio_free);
#endif
  return (void *)result;
}

/* *****************************************************************************
Main function
***************************************************************************** */

int main(int argc, char const *argv[]) {
  fio_cli_start(
      argc,
      argv,
      0,
      0,
      "This program speed tests fio_malloc vs. system's malloc.\n"
      "It also tests for failed allocations as reported to the user.\n"
      "It does not test the actual allocators, this is performed in the STL "
      "test unit.\n\n"
      "the following arguments are available:",
      FIO_CLI_INT(
          "--threads -t (1) runs the test concurrently, adding contention. "),
      FIO_CLI_INT(
          "--cycles -c (8) the amount of times to run through the test."),
      FIO_CLI_INT(
          "--start-from -s (64) the minimal amount of bytes to allocate "
          "(rounded up by 16)."),
      FIO_CLI_INT("--end-at -e (65536) the maximum amount of bytes to allocate "
                  "(rounded up by 16)."),
      FIO_CLI_PRINT("Note (due to realloc testing):"),
      FIO_CLI_PRINT(
          "maximum amount of bytes allocated is at least twice the minimal."),
      FIO_CLI_BOOL(
          "--warmup -w perform a warmup cycle before testing allocator."));

  TEST_CYCLES_REPEAT = (size_t)fio_cli_get_i("-c");
  TEST_CYCLES_START = (15 + (size_t)fio_cli_get_i("-s")) >> 4;
  TEST_CYCLES_END = (15 + (size_t)fio_cli_get_i("-e")) >> 5;
  size_t warmup = fio_cli_get_bool("-w");
  if (TEST_CYCLES_START >= TEST_CYCLES_END)
    TEST_CYCLES_END = TEST_CYCLES_START + 1;
  if (!TEST_CYCLES_REPEAT)
    TEST_CYCLES_REPEAT = 1;

#if DEBUG
  fprintf(stderr,
          "\n=== WARNING: performance tests using the DEBUG mode are "
          "invalid. \n");
#endif

  size_t thread_count = fio_cli_get_i("-t") ? fio_cli_get_i("-t") : 1;
  if (thread_count == (size_t)-1) {
    thread_count = fio_malloc_arenas();
  } else if (thread_count > 64) {
    FIO_LOG_ERROR("thread count cannot exceed 64 threads when testing.");
    thread_count = 64;
  }
  fio_cli_end();
  fio_free(fio_malloc(16)); /* initialize allocator if needed */
  free(malloc(16));         /* initialize allocator if needed */
#if _MSC_VER
  fio_thread_t threads[100];
  FIO_ASSERT(thread_count < 100,
             "Windows MSVC has anooying limitations and defaults.");

#else
  fio_thread_t threads[thread_count];
#endif

  fprintf(stderr, "========================================\n");
  fprintf(stderr, FIO_MALLOC_TEST_NOTICE "\n");
  fio_malloc_print_settings();

  /* test facil.io allocations */
  fprintf(stderr, "========================================\n");
  if (fio_realloc_is_safe()) {
    fprintf(stderr,
            "NOTE: This tests uses a facil.io allocator that initializes all  "
            "memory to zero, always.\n"
            "\n      In contrast, the system allocator may return (and retain) "
            "junk data.\n"
            "\n      This added security feature incurs a performance "
            "penalty.\n\n");
  } else {
    fprintf(stderr,
            "NOTE: This test uses a facil.io allocator that does NOT "
            "initialize memory.\n"
            "\n     This is the standard behavior for memory allocators (is "
            "but shouldn't be).\n"
            "\n     When initializing memory, which is facil.io's recommended "
            "default,\n"
            "     there's an added performance cost.\n\n");
  }
  fprintf(stderr,
          "Test allocation ranges: %zu - %zu bytes.\n",
          ((size_t)(TEST_CYCLES_START) << 4),
          ((size_t)(TEST_CYCLES_END) << 5));

  fprintf(stderr, "========================================\n");
  fprintf(stderr,
          "Performance Testing facil.io memory allocator with %zu threads "
          "(please wait):\n\n",
          thread_count);
  if (warmup) {
    fprintf(stderr, "Warming up... ");
    for (size_t i = 0; i < thread_count; ++i) {
      FIO_ASSERT(fio_thread_create(threads + i, test_facil_malloc, NULL) == 0,
                 "Couldn't spawn thread.");
    }
    for (size_t i = 0; i < thread_count; ++i) {
      FIO_ASSERT(fio_thread_join(threads + i) == 0,
                 "Couldn't join thread %zu. errno: %d",
                 i,
                 errno);
    }
    test_mem_functions(NULL, calloc, NULL, NULL);
    fprintf(stderr, "Finished.\n");
  }

  for (size_t i = 0; i < thread_count; ++i) {
    FIO_ASSERT(fio_thread_create(threads + i, test_facil_malloc, NULL) == 0,
               "Couldn't spawn thread.");
  }
  for (size_t i = 0; i < thread_count; ++i) {
    FIO_ASSERT(fio_thread_join(threads + i) == 0,
               "Couldn't join thread %zu. errno: %d",
               i,
               errno);
  }
  test_mem_functions(NULL, NULL, NULL, NULL);

  /* test system allocations */
  fprintf(stderr, "========================================\n");
  fprintf(stderr,
          "Performance Testing system memory allocator with %zu threads "
          "(please wait):\n\n",
          thread_count);
  if (warmup) {
    fprintf(stderr, "Warming up... ");
    for (size_t i = 0; i < thread_count; ++i) {
      FIO_ASSERT(fio_thread_create(threads + i, test_system_malloc, NULL) == 0,
                 "Couldn't spawn thread.");
    }
    for (size_t i = 0; i < thread_count; ++i) {
      FIO_ASSERT(fio_thread_join(threads + i) == 0,
                 "Couldn't join thread %zu. errno: %d",
                 i,
                 errno);
    }
    test_mem_functions(NULL, calloc, NULL, NULL);
    fprintf(stderr, "Finished.\n");
  }
  for (size_t i = 0; i < thread_count; ++i) {
    FIO_ASSERT(fio_thread_create(threads + i, test_system_malloc, NULL) == 0,
               "Couldn't spawn thread.");
  }
  for (size_t i = 0; i < thread_count; ++i) {
    FIO_ASSERT(fio_thread_join(threads + i) == 0,
               "Couldn't join thread %zu. errno: %d",
               i,
               errno);
  }
  test_mem_functions(NULL, NULL, NULL, NULL);

  return 0; // fio_cycles > sys_cycles;
}
