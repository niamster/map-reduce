#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include <stdlib.h>
#include <time.h>

#include "utils.h"

void test_issep(void) {
    CU_ASSERT_TRUE(is_sep(' '));
    CU_ASSERT_TRUE(is_sep(':'));
    CU_ASSERT_FALSE(is_sep('\0'));
    CU_ASSERT_FALSE(is_sep('/'));
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

   if (!(suite = CU_add_suite("basic", init_suite, clean_suite)))
       goto out;

   if (!(CU_add_test(suite, "issep", test_issep)))
       goto out;

   CU_basic_run_tests();

  out:
   CU_cleanup_registry();
   return CU_get_error();
}
