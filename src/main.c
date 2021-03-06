/*
  Copyright 2016 milinevskyy@gmail.com

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "utils.h"
#include "mapred.h"

void usage(const char *prog) {
    printf("%s <path> <N>\n", prog);
    printf("<path>  :   path to the file with words\n");
    printf("<N>     :   number of `map` thread (greater than zero)\n");
    exit(1);
}

static void wc_map(mr_t *mr, const char *data, size_t size, void *user) {
    (void)user;
    fchunk_read_word(data, size, _mr_chunk_process_word, mr);
}

static void wc_reduce(mr_t *mr, ukey_t *key, unsigned long entry, void *user) {
    unsigned long count = 0;
    olentry_t el;
    int res;
    (void)user;

    res = mr_get_entry(mr, key, entry, &el);
    if (res != 0)
        die("Failed to get entry: %d\n", res);

    while (true) {
        ++count;
        if (el.next == OLIST_NONE)
            break;

        res = mr_get_entry(mr, key, el.next, &el);
        if (res != 0)
            die("Failed to get entry: %d\n", res);
    }

    res = mr_emit(mr, key, (void *)count);
    if (res != 0)
        die("Failed to emit a key: %d\n", res);
}

static void wc_output(ukey_t *key, olentry_t *el, void *user) {
    (void)user;

    printf("%s=%lu\n", key->key, (unsigned long)el->value);
}


int main(int argc, char **argv) {
    const char *prog = argv[0];
    const char *file = NULL;
    int threads = 0;
    FILE *fh = NULL;
    mr_t mr;
    int res;

    if (argc != 3)
        usage(prog);
    file = argv[1];
    threads = atoi(argv[2]);

    if (threads <= 0)
        usage(prog);

    fh = fopen(file, "r");
    if (!fh)
        die("Failed to open %s: %s\n", file, strerror(errno));

    res = mr_init(&mr, threads);
    if (res != 0)
        die("Failed to init MR: %d\n", res);
    res = mr_process_fd(&mr, fileno(fh), wc_map, wc_reduce, wc_output, NULL);
    if (res != 0)
        die("Failed to process MR: %d\n", res);
    mr_destroy(&mr);

    fclose(fh);

    return 0;
}
