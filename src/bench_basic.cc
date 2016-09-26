#include <hayai/hayai.hpp>
#include <unistd.h>
#include <assert.h>

#include "mapred.h"

#include "bench_basic.h"

typedef struct {
    unsigned long total;
} _basic_data_t;

static void _basic_map(mr_t *mr, ukey_t *key, void *user) {
    int res;
    (void)user;

    res = mr_emit_intermediate(mr, key, NULL);
    if (res != 0)
        die("Failed to emit an intermediate key: %d\n", res);
}

static void _basic_reduce(mr_t *mr, ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    unsigned long count = 0;
    olentry_t *el = entries;
    int res;
    (void)user;

    while (true) {
        ++count;
        if (el->next == -1)
            break;
        el = &values[el->next];
    }

    res = mr_emit(mr, key, (void *)count);
    if (res != 0)
        die("Failed to emit a key: %d\n", res);
}

static void _basic_output(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    olentry_t *el = entries;
    _basic_data_t *bdata = (_basic_data_t *)user;
    (void)values, (void)key;

    bdata->total += (unsigned long)el->value;
}

class mrFixture : public ::hayai::Fixture {
  public:
    virtual void SetUp() {
        data = new char[src_bench_basic_data_len*copies];
        assert(data);
        unsigned idx; for (idx=0; idx<copies; ++idx) {
            memcpy(data+idx*dlen, src_bench_basic_data, dlen);
        }
    }

    void __run(unsigned threads) {
        _basic_data_t bdata = {.total = 0};
        mr_t mr;

        assert(mr_init(&mr, threads) == 0);
        assert(mr_process(&mr, data, copies*dlen, _basic_map, _basic_reduce, &bdata) == 0);
        assert(wtable_iterate(&mr.output, _basic_output, &bdata) == 0);
        mr_destroy(&mr);

        assert(bdata.total == 5000*copies);
    }

    virtual void TearDown() {
        delete [] data;
    }

    char *data;
    const unsigned dlen = src_bench_basic_data_len;
    const unsigned copies = 10*1024*1024/dlen;
};

BENCHMARK_P_F(mrFixture, mapred, 1, 10,
    (unsigned threads)) {
    __run(threads);
}

BENCHMARK_P_INSTANCE(mrFixture, mapred, (1));
BENCHMARK_P_INSTANCE(mrFixture, mapred, (sysconf(_SC_NPROCESSORS_ONLN)));
BENCHMARK_P_INSTANCE(mrFixture, mapred, (sysconf(_SC_NPROCESSORS_ONLN)/2));
BENCHMARK_P_INSTANCE(mrFixture, mapred, (sysconf(_SC_NPROCESSORS_ONLN)*2));
