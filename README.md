# Rationale
This program counts the occurrence of the words in the file.

# Synopsis
$ ./mapred file N
­ file: a file name containing words (can be generated here:http://www.lipsum.com/feed/html )
­ N : a number of threads.
The program will launch N threads (using pthreads) to share the work as equally as possible
(map phase).
The program will then collect the results of all the threads (reduce phase), aggregate all the
results, and display them, sorted by word.
A word is a sequence of characters between 2 separators. These separators are start and end
of file, tabulations, spaces, punctuation, and end of line.

# Example
Of the file is the following:
foo bar qux
bar bar baz
And we launch the program with e.g. N=2, the program shall display:
bar=3
baz=1
foo=1
qux=1
