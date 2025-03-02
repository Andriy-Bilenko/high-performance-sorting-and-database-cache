# high-performance-sorting-and-database-cache
Current project features 2 parts:
1. efficient external sorting under memory and processor constraints 
2. multi-threaded database cache, built for scalability and low-latency systems

task description can be found in `task_description.md` in root directory of this repository.

## sorting
### build / run / test
```bash
cd sorting
# compile
g++ -o 1G_file_generator 1G_file_generator.cpp
g++ -o sorter sorter.cpp
g++ -o check_sorted check_sorted.cpp
# run
./1G_file_generator unsorted_1GB.txt # generates .txt file of random double-precision numbers of size 1 GB
./sorter unsorted_1GB.txt sorted_1GB.txt # sorts random numbers and outputs to final .txt file
./check_sorted sorted_1GB.txt # checks whether numbers in a file are really sorted correctly
```
to run sorting on a single-core processor with a clock speed of 2GHz:
```bash
# installing tools on arch linux
sudo pacman -S linux-tools 
# check current mode to revert back in future
cpupower frequency-info 
# set max CPU frequency to 2GHz
sudo cpupower frequency-set --max 2GHz 
# check it worked
cat /proc/cpuinfo | grep "cpu MHz" 
cpupower frequency-info
# run sorting on core 0
taskset -c 0 ./sorter unsorted_1GB.txt sorted_1GB.txt
# revert changes back
sudo cpupower frequency-set --max 4GHz 
cat /proc/cpuinfo | grep "cpu MHz"
cpupower frequency-info 
```
expected output:
```
# unsorted file generation
File 'unsorted_1GB.txt' generated successfully (1024 MB) in 62.3764 seconds.

# sorting (single-core 2GHz)
Sorting completed successfully in 202.598 seconds.

# checking
The sorted file is in correct order.
```
profiling sorting:
```bash
g++ -g -o sorter sorter.cpp # compile with a debug flag
valgrind --tool=massif ./sorter unsorted_1GB.txt sorted_1GB.txt # run profiler
# see profiler output
ms_print massif.out.xxxxxx
# or 
massif-visualizer massif.out.xxxxxx
```
At first I tried limiting controlled heap allocations to 50 MB (the task is to go under 100 MB) to test how well it goes, then expanded to 90 MB and peak memory usage is 90.1 MB.
You can inspect massif profiling logs `massif.out.50MB` and `massif.out.90MB` in `sorting/`.

### notes
- **Generator of unsorted 1 GB file with double-precision numbers**

    Using `std::random_device` (seed) and `std::mt19937_64` (Mersenne Twister pseudorandom number generator, seeded by the random device) ensures high-quality random numbers in the range [1.0, 1.0e308].

- **Sorter**

    Uses external sorting to sort huge dataset. Loads divided data into memory in chunks of 90 MB (limiting RAM usage), sorts them using the standard C++ `std::sort` and writes to temporary files on a disk. After that uses priority queue (min-heap) from STL to merge data from temporary files in a single sorted file.

## multithreaded DB cache
### build and run
```bash
cd cache
# compile
g++ -o db_cache db_cache.cpp
# create DB file
touch test_db.txt
# run
./db_cache test_db.txt 10 5 # <executable> <DB file> <max num of cache elements> <num of threads>
```
executable runs some arbitrary multithreading test code for debugging purposes. 
If maximum number of cache elements is 0 or less - cache is not being created.

### notes
This part includes:
1. Abstract structure `i_db` for a database interface

    given from `task_description.md`

2. Text file database interface (`CachedFileDatabase` class) implementing `i_db`. It has:

    1. thread local variables for uncommited DB changes for each thread
    2. optional cache struct with O(1) insertion, deletion, lookup and modification
    3. mutexes as a synchronisation mechanism for thread-safety and ACID compliance

3. `Cache` structure caching get, set and delete queries, keeping most recent ones at the top

    combines `std::list` to maintain order of cache items (most recently used are at the front, least used ones are getting overwritten) with `std::unordered_map` for fast O(1) access to elements based on their keys

Low-level file database delete/write/overwrite operations are not optimised in current implementation, it is better to use SQL databases instead.



If you ran into some issues by any chance or need to contact the developer, it would be great to recieve your valuable feedback on email: *bilenko.a.uni@gmail.com*.

<div align="right">
<table><td>
<a href="#start-of-content">↥ Scroll to top</a>
</td></table>
</div>
