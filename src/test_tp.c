#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include <stdlib.h>
#include <time.h>

#include "tp.h"

void test_init(void) {
    tp_t tp;

    CU_ASSERT_EQUAL_FATAL(tp_init(&tp, 10), 0);
    tp_destroy(&tp);
}

void __test_launch(void *user) {
    unsigned *count = user;
    __sync_fetch_and_add(count, 1);
}

void test_launch(void) {
    unsigned idx;
    unsigned count = 0;
    const unsigned max = 30;
    tp_t tp;

    CU_ASSERT_EQUAL_FATAL(tp_init(&tp, 10), 0);

    for (idx=0; idx<max; ++idx)
        CU_ASSERT_EQUAL_FATAL(tp_push(&tp, __test_launch, &count), 0);
    CU_ASSERT_EQUAL_FATAL(tp_sync(&tp), 0);
    CU_ASSERT_EQUAL_FATAL(__sync_fetch_and_add(&count, 0), max);

    tp_destroy(&tp);
}

int init_suite(void) {
    srand((unsigned)time(NULL));

    return 0;
}

int clean_suite(void) {
    return 0;
}

int main(int argc, char **argv) {
   CU_pSuite suite = NULL;
   (void)argc, (void)argv;

   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   if (!(suite = CU_add_suite("tp", init_suite, clean_suite)))
       goto out;

   if (!(CU_add_test(suite, "init", test_init)))
       goto out;

   if (!(CU_add_test(suite, "launch", test_launch)))
       goto out;

   CU_basic_run_tests();

  out:
   CU_cleanup_registry();
   return CU_get_error();
}
