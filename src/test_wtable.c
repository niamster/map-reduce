#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include <stdlib.h>
#include <sys/param.h>
#include <time.h>
#include <assert.h>

#include "utils.h"

#include "wtable.h"

void test_init(void) {
    wtable_t wtable;

    CU_ASSERT_EQUAL_FATAL(wtable_init(&wtable, 1), 0);
    wtable_destroy(&wtable);
}

typedef struct {
    unsigned uniq;
    unsigned total;
    const char *prev;
} insert_iter_data_t;

void __test_insert_iter(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    insert_iter_data_t *data = user;
    olentry_t *el = entries;

    CU_ASSERT_PTR_NOT_NULL_FATAL(key);
    CU_ASSERT_PTR_NOT_NULL_FATAL(entries);
    assert(data);

    if (data->prev) {
        assert(data->prev && key); // make clang analyzer happy
        CU_ASSERT_FATAL(strcmp(data->prev, key->key) < 0);
    }

    ++data->uniq;
    while (true) {
        assert(el && el->key && key); // make clang analyzer happy
        CU_ASSERT_EQUAL_FATAL(strcmp(el->key->key, key->key), 0);
        ++data->total;
        if (el->next == -1)
            break;
        el = &values[el->next];
    }
    data->prev = key->key;
}

void __test_insert_items(wtable_t *wtable, const char *items[], size_t count) {
    unsigned idx;

    for (idx=0; idx<count; ++idx) {
        ukey_t *key = ukey_init(items[idx], strlen(items[idx]));
        CU_ASSERT_PTR_NOT_NULL_FATAL(key);
        CU_ASSERT_EQUAL_FATAL(wtable_insert(wtable, key, NULL), 0);
        ukey_put(key);
    }
}

void test_insert(void) {
    const char *items[] = {
        "a", "b", "c", "d", "e", "f",
    };
    insert_iter_data_t udata;
    const unsigned bits = 8;
    wtable_t wtable;

    unsigned idx; for (idx=0; idx<=ARRAY_SIZE(items); ++idx) {
        CU_ASSERT_EQUAL_FATAL(wtable_init(&wtable, bits), 0);

        memset(&udata, 0, sizeof udata);
        CU_ASSERT_EQUAL_FATAL(wtable_iterate(&wtable, __test_insert_iter, &udata), 0);
        CU_ASSERT_EQUAL_FATAL(udata.uniq, 0);
        CU_ASSERT_EQUAL_FATAL(udata.total, 0);
        CU_ASSERT_PTR_NULL_FATAL(udata.prev);

        __test_insert_items(&wtable, items, idx);

        memset(&udata, 0, sizeof udata);
        CU_ASSERT_EQUAL_FATAL(wtable_iterate(&wtable, __test_insert_iter, &udata), 0);
        CU_ASSERT_EQUAL_FATAL(udata.uniq, idx);
        CU_ASSERT_EQUAL_FATAL(udata.total, idx);
        if (idx > 0)
            CU_ASSERT_EQUAL_FATAL(strcmp(udata.prev, items[idx-1]), 0);

        wtable_destroy(&wtable);
    }
}

void test_insert_dup(void) {
    const char *samples[] = {
        "a", "b", "c",
    };
    insert_iter_data_t udata;
    const unsigned max = MAX(rand()%1024, ARRAY_SIZE(samples)*3);
    const char **items = calloc(1, max * sizeof(char *));
    const unsigned bits = 8;
    unsigned idx;
    wtable_t wtable;

    CU_ASSERT_PTR_NOT_NULL_FATAL(items);

    assert(items && max > 0); // make clang analyzer happy
    for (idx=0; idx<max; ++idx)
        items[idx] = samples[rand()%ARRAY_SIZE(samples)];
    for (idx=0; idx<ARRAY_SIZE(samples); ++idx)
        items[rand()%max] = samples[idx];

    CU_ASSERT_EQUAL_FATAL(wtable_init(&wtable, bits), 0);

    __test_insert_items(&wtable, items, max);

    memset(&udata, 0, sizeof udata);
    CU_ASSERT_EQUAL_FATAL(wtable_iterate(&wtable, __test_insert_iter, &udata), 0);
    CU_ASSERT_EQUAL_FATAL(udata.uniq, ARRAY_SIZE(samples));
    CU_ASSERT_EQUAL_FATAL(udata.total, max);

    wtable_destroy(&wtable);

    free(items);
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

   if (!(suite = CU_add_suite("wtable", init_suite, clean_suite)))
       goto out;

   if (!(CU_add_test(suite, "init", test_init)))
       goto out;

   if (!(CU_add_test(suite, "insert", test_insert)))
       goto out;

   if (!(CU_add_test(suite, "insert_dup", test_insert_dup)))
       goto out;

   CU_basic_run_tests();

  out:
   CU_cleanup_registry();
   return CU_get_error();
}
