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

void usage(const char *prog) {
    printf("%s <path> <N>\n", prog);
    printf("<path>  :   path to the file with words\n");
    printf("<N>     :   number of `map` thread (greater than zero)\n");
    exit(1);
}

int main(int argc, char **argv) {
    const char *prog = argv[0];
    const char *file = NULL;
    int threads = 0;
    FILE *fh = NULL;

    if (argc != 3)
        usage(prog);
    file = argv[1];
    threads = atoi(argv[2]);

    if (threads <= 0)
        usage(prog);

    fh = fopen(file, "r");
    if (!fh)
        die("Failed to open %s: %s\n", file, strerror(errno));

    fclose(fh);

    return 0;
}
