# wallClockProfiler
Sadly, this apparently ONLY profiler in existence on Linux that actually works.

By "actually works," I mean that a profiler is supposed to give you a rough estimate where your program is spending the most time.  Slow programs are slow for a variety of reasons, not just because of inefficient algorithms that waste CPU cycles.  Lots of programs are I/O bound.  If you profile such a program, you want to know where the I/O hotspots are, and you don't care where the CPU hotspots are, because they are tiny, by comparison.

As far as I can tell, after way too many hours of researching and testing every available tool, there is nothing on Linux that actually does this well.  I had a simple test program, written in C++, that looped over a big file on disk, fseeking randomly and reading from the file a bunch of times.  Obviously, such a program is spending almost all of its time in fseek.  But not a single existing profiler would tell me this.

Not perf.  Not gperftools.  Not OProfile.  Obviously not gprof.  None of the usual suspects.

Some of them were telling me that the majority of the running time was spent in rand (which was used to pick the random offset to fseek to).  The truth was, this was only about 1% of the simple program's wall-clock runtime.  If I want to speed this program up, optimizing rand is going to be a waste of time.

Valgrind's callgrind can provide better results than the others, but it makes the program 10x slower (or something like that), so it's useless in production environments (for example, profiling a live server that has real users connecting to it).

And it turns out that what you actually want to do is really pretty simple.  So simple that some very smart people have suggested that you do it manually:

> Attach to your program in gdb.  Interrupt it at random a bunch of times.  See what stack traces repeat more often than you'd expect them to by random chance.  If 50% of your random interrupts land in the same exact stack trace, there's a good chance your program is spending a lot of time there.  Some people call this "poor man's profiling."


This profiler is a simple bit of C++ code that automates this task.  And it uses GDB, so it gets stack traces for you that are as real and informative as stack traces from GDB.  If your program is GDB-debuggable, this profiler will work on it.

And besides GDB and standard Linux system includes, it has no dependencies of any kind.  It's also a single CPP file, a file that's so simple that it doesn't even need a build script, just:

```
g++ -o wallClockProfiler wallClockProfiler.cpp 
```
You don't need to rebuild or relink your program to profile it (though building your program with debugging symbols would probably be helpful).

It can either run your target program directly, or attach to an existing process.  It can sample however many times per second you want it to, and it can detach automatically after a certain number of seconds.

For example, if your server process is experiencing heavy load right now, you can attach to it for the next ten seconds, grab a few hundred stack samples, then see a nice little text report telling you exactly where your server is spending its time right now.

Overhead scales with your chosen sampling rate.  If you're looking for a big problem, a relatively low sampling rate (and thus a low overhead) will be sufficient to catch it.  For example, With 20 samples per second, I'm seeing a 12% slowdown on my test program.

## Examples

Run ./myProgram and sample the stack 20 times per second until it exits:
```
./wallClockProfiler 20 ./myProgram
```

Attatch to an existing process ./myProgram (PID 3042) and sample the stack 20 times per second for 60 seconds:
```
./wallClockProfiler 20 ./myProgram 3042 60
```

## Sample Output
From profiling the aforementioned test program that does a lot of random fseeks:
```
Sampling stack while program runs...
Sampling 20 times per second, for 50000 usec between samples
Program exited normally
313 stack samples taken
3 unique stacks sampled



Report:

81.789% =====================================
        1: __kernel_vsyscall   (at :-1)
        2: __read_nocancel   (at ../sysdeps/unix/syscall-template.S:81)
        3: _IO_new_file_seekoff   (at fileops.c:1079)
        4: _IO_seekoff_unlocked   (at ioseekoff.c:69)
        5: __GI_fseek   (at fseek.c:39)
        6: readRandFileValue   (at testProf.cpp:15)
        7: main   (at testProf.cpp:41)


17.572% =====================================
        1: __kernel_vsyscall   (at :-1)
        2: __llseek   (at ../sysdeps/unix/sysv/linux/llseek.c:33)
        3: __GI__IO_file_seek   (at fileops.c:1214)
        4: _IO_new_file_seekoff   (at fileops.c:1072)
        5: _IO_seekoff_unlocked   (at ioseekoff.c:69)
        6: __GI_fseek   (at fseek.c:39)
        7: readRandFileValue   (at testProf.cpp:15)
        8: main   (at testProf.cpp:41)


 0.319% =====================================
        1: __GI__dl_debug_state   (at dl-debug.c:74)
        2: dl_main   (at rtld.c:2305)
        3: _dl_sysdep_start   (at ../elf/dl-sysdep.c:249)
        4: _dl_start_final   (at rtld.c:332)
        5: _dl_start   (at rtld.c:558)
        6: _start   (at :-1)
```
You can see that 99% of the samples occurred down inside `__GI-fseek`.  No existing profiler showed me anything like this.

And how do I know whether this profile is accurate?  Maybe its just as inaccurate as the others (below), just in a different way.

Here's where the rubber meets the road in terms of program execution time.  By commenting out the fseek calls (and their corresponding fgetc calls) that were found above, the execution time for the test program drops from 14 seconds down to 0.25 seconds.  In other words, these I/O calls are indeed responsible for 99% of the execution time.

## Sample output from other profilers

### gprof
From calling:
```
g++ -pg -g -o testProf testProf.cpp
./testProf
gprof ./testProf
gprof -l ./testProf
```
Output:
```
Flat function profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total           
 time   seconds   seconds    calls  ns/call  ns/call  name    
 55.88      0.10     0.10 10000000     9.50     9.50  getRandomBoundedInt(int, int)
 38.24      0.16     0.07 10000000     6.50    16.00  readRandFileValue(_IO_FILE*)
  5.88      0.17     0.01                             main
  0.00      0.17     0.00        1     0.00     0.00  _GLOBAL__sub_I_MAX
  0.00      0.17     0.00        1     0.00     0.00  __static_initialization_and_destruction_0(int, int)
  
  
Flat line profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total           
 time   seconds   seconds    calls  ns/call  ns/call  name    
 23.53      0.04     0.04                             readRandFileValue(_IO_FILE*) (testProf.cpp:26 @ 80486d0)
 23.53      0.08     0.04                             getRandomBoundedInt(int, int) (testProf.cpp:14 @ 8048677)
 11.76      0.10     0.02                             readRandFileValue(_IO_FILE*) (testProf.cpp:24 @ 80486b5)
 11.76      0.12     0.02                             getRandomBoundedInt(int, int) (testProf.cpp:10 @ 8048658)
 11.76      0.14     0.02                             getRandomBoundedInt(int, int) (testProf.cpp:16 @ 80486a0)
  5.88      0.15     0.01 10000000     1.00     1.00  getRandomBoundedInt(int, int) (testProf.cpp:9 @ 804864d)
  2.94      0.15     0.01 10000000     0.50     0.50  readRandFileValue(_IO_FILE*) (testProf.cpp:23 @ 80486aa)
  2.94      0.16     0.01                             getRandomBoundedInt(int, int) (testProf.cpp:17 @ 80486a8)
  2.94      0.17     0.01                             main (testProf.cpp:42 @ 8048738)
  2.94      0.17     0.01                             main (testProf.cpp:43 @ 8048742)
  0.00      0.17     0.00        1     0.00     0.00  _GLOBAL__sub_I_MAX (testProf.cpp:50 @ 80487c9)
  0.00      0.17     0.00        1     0.00     0.00  __static_initialization_and_destruction_0(int, int) (testProf.cpp:50 @ 804878c)

```

### perf
From calling:
```
perf record -g ./testProf
perf report
```
Output:
```
Samples: 59K of event 'cycles', Event count (approx.): 3556233224               
+  47.12%  testProf  [kernel.kallsyms]  [k] __copy_to_user_ll
+   9.35%  testProf  [kernel.kallsyms]  [k] find_get_page
+   5.12%  testProf  [kernel.kallsyms]  [k] radix_tree_lookup_element
+   3.95%  testProf  [kernel.kallsyms]  [k] native_flush_tlb_single
+   3.51%  testProf  [kernel.kallsyms]  [k] sysenter_past_esp
+   3.36%  testProf  [kernel.kallsyms]  [k] generic_file_aio_read
+   2.37%  testProf  [vdso]             [.] 0x00000428
+   2.13%  testProf  libc-2.19.so       [.] _IO_file_seekoff@@GLIBC_2.1
+   1.25%  testProf  [kernel.kallsyms]  [k] ext4_llseek
+   1.24%  testProf  [kernel.kallsyms]  [k] fget_light
+   1.09%  testProf  [kernel.kallsyms]  [k] sys_llseek
+   1.08%  testProf  [kernel.kallsyms]  [k] _copy_to_user
+   1.05%  testProf  libc-2.19.so       [.] _IO_getc
+   1.02%  testProf  [kernel.kallsyms]  [k] common_file_perm
+   0.99%  testProf  [kernel.kallsyms]  [k] do_sync_read
+   0.98%  testProf  testProf           [.] getRandomBoundedInt(int, int)
+   0.91%  testProf  [kernel.kallsyms]  [k] vfs_read
+   0.88%  testProf  [kernel.kallsyms]  [k] current_kernel_time
+   0.77%  testProf  [kernel.kallsyms]  [k] fsnotify
+   0.75%  testProf  [kernel.kallsyms]  [k] kmap_atomic_prot
+   0.73%  testProf  [kernel.kallsyms]  [k] file_read_actor
+   0.71%  testProf  libc-2.19.so       [.] fseek
```
Enabling callgraphis in the profile and output like so:
```
perf record --call-graph fp ./testProf
perf report --call-graph --stdio
```
gives us a bit more information in the profile, but it's still not that useful, and still doesn't pinpoint fseek:
```
# Overhead   Command      Shared Object                                      Sym
# ........  ........  .................  .......................................
#
    46.05%  testProf  [kernel.kallsyms]  [k] __copy_to_user_ll                  
            |
            --- __copy_to_user_ll
               |          
               |--97.14%-- file_read_actor
               |          generic_file_aio_read
               |          do_sync_read
               |          vfs_read
               |          sys_read
               |          sysenter_after_call
               |          0xb77b2428
               |          |          
               |           --100.00%-- main
               |                     __libc_start_main
               |          
               |--2.48%-- _copy_to_user
               |          sys_llseek
               |          sysenter_after_call
               |          0xb77b2428
                --0.39%-- [...]

     9.13%  testProf  [kernel.kallsyms]  [k] find_get_page                      
            |
            --- find_get_page
               |          
               |--99.26%-- generic_file_aio_read
               |          do_sync_read
               |          vfs_read
               |          sys_read
               |          sysenter_after_call
               |          0xb77b2428
               |          |          
               |           --100.00%-- main
               |                     __libc_start_main
               |          
                --0.74%-- do_sync_read
                          vfs_read
                          sys_read
                          sysenter_after_call
                          0xb77b2428

     5.24%  testProf  [kernel.kallsyms]  [k] radix_tree_lookup_element          
            |
            --- radix_tree_lookup_element
               |          
               |--99.08%-- radix_tree_lookup_slot
               |          find_get_page
               |          generic_file_aio_read
               |          do_sync_read
               |          vfs_read
               |          sys_read
               |          sysenter_after_call
               |          0xb77b2428
               |          
                --0.92%-- find_get_page
                          generic_file_aio_read
                          do_sync_read
                          vfs_read
                          sys_read
                          sysenter_after_call
                          0xb77b2428
```
### gperftools
From calling:

```
LD_PRELOAD=/usr/local/lib/libprofiler.so CPUPROFILE=prof.out ./testProf
pprof --text ./testProf ./prof.out
```
Output:
```
Total: 1456 samples
    1339  92.0%  92.0%     1339  92.0% 0xb7737428
      30   2.1%  94.0%       30   2.1% _IO_new_file_seekoff
      15   1.0%  95.1%       15   1.0% _IO_getc
      14   1.0%  96.0%       14   1.0% getRandomBoundedInt
       8   0.5%  96.6%        8   0.5% __random_r
       7   0.5%  97.0%        7   0.5% _IO_acquire_lock_fct
       7   0.5%  97.5%        7   0.5% _IO_seekoff_unlocked
       6   0.4%  97.9%        6   0.4% 0xb7737429
       6   0.4%  98.4%        6   0.4% __GI_fseek
       4   0.3%  98.6%        4   0.3% __GI__IO_file_seek
       4   0.3%  98.9%        4   0.3% __llseek
       4   0.3%  99.2%       25   1.7% readRandFileValue
       3   0.2%  99.4%        3   0.2% __random
       3   0.2%  99.6%       57   3.9% main
       2   0.1%  99.7%        2   0.1% __read_nocancel
       2   0.1%  99.9%        3   0.2% _init
       1   0.1%  99.9%        1   0.1% 0xb773741d
       1   0.1% 100.0%        1   0.1% rand
       0   0.0% 100.0%       57   3.9% __libc_start_main
```
Here at least we see that 92% of our runtime was... down inside some mystery function.  But that mystery function was called from somewhere, right?  Even outputing to a full visual callgraph viewer (using the `--callgrind` output option for pprof) still shows that 92% of our execution time was spent in a function with no callers.

Some sources suggest setting CPUPROFILE_REALTIME=1 to enable wall-clock based sampling, but I found that it made no difference in the resulting profile.

### oprofile
From calling:
```
operf ./testProf
opreport --symbols
```
Output:
```
samples  %        image name               symbol name
180437   89.2334  no-vmlinux               /no-vmlinux
5312      2.6270  libc-2.19.so             _IO_file_seekoff@@GLIBC_2.1
4554      2.2521  [vdso] (tgid:6710 range:0xb7761000-0xb7761fff) [vdso] (tgid:6710 range:0xb7761000-0xb7761fff)
1961      0.9698  testProf                 getRandomBoundedInt(int, int)
1804      0.8922  libc-2.19.so             fseek
1457      0.7205  libc-2.19.so             fgetc
1030      0.5094  libc-2.19.so             random_r
978       0.4837  libc-2.19.so             _IO_seekoff_unlocked
842       0.4164  libc-2.19.so             llseek
712       0.3521  libc-2.19.so             random
687       0.3397  libc-2.19.so             __x86.get_pc_thunk.bx
568       0.2809  testProf                 readRandFileValue(_IO_FILE*)
547       0.2705  libc-2.19.so             _IO_file_seek
428       0.2117  testProf                 main
330       0.1632  libc-2.19.so             _IO_file_read
250       0.1236  libc-2.19.so             rand
232       0.1147  libc-2.19.so             __read_nocancel
56        0.0277  libc-2.19.so             read
8         0.0040  libc-2.19.so             __uflow
5         0.0025  libc-2.19.so             _IO_switch_to_get_mode
3         0.0015  libc-2.19.so             _IO_default_uflow
2        9.9e-04  ld-2.19.so               _dl_lookup_symbol_x
2        9.9e-04  libc-2.19.so             _IO_file_underflow@@GLIBC_2.1
1        4.9e-04  ld-2.19.so               dl_main
1        4.9e-04  libc-2.19.so             _dl_addr
1        4.9e-04  libc-2.19.so             malloc_init_state
```

### Vallgrind callgrind
From calling:
```
valgrind --tool=callgrind --collect-systime=yes ./testProf
callgrind_annotate callgrind.out.6218
```
Output:
```
--------------------------------------------------------------------------------
           Ir   sysCount sysTime 
--------------------------------------------------------------------------------
5,380,492,114 19,999,640  50,476  PROGRAM TOTALS

--------------------------------------------------------------------------------
           Ir   sysCount sysTime  file:function
--------------------------------------------------------------------------------
1,689,942,527          .       .  /build/buildd/eglibc-2.19/libio/fileops.c:_IO_
file_seekoff@@GLIBC_2.1 [/lib/i386-linux-gnu/libc-2.19.so]
  430,000,000          .       .  /build/buildd/eglibc-2.19/libio/fseek.c:fseek 
[/lib/i386-linux-gnu/libc-2.19.so]
  429,999,996          .       .  /build/buildd/eglibc-2.19/libio/ioseekoff.c:_I
O_seekoff_unlocked [/lib/i386-linux-gnu/libc-2.19.so]
  420,013,020          .       .  /build/buildd/eglibc-2.19/stdlib/random_r.c:ra
ndom_r [/lib/i386-linux-gnu/libc-2.19.so]
  350,002,466          .       .  /build/buildd/eglibc-2.19/libio/getc.c:getc [/
lib/i386-linux-gnu/libc-2.19.so]
  320,000,004          .       .  testProf.cpp:getRandomBoundedInt(int, int) [/h
ome/jasonrohrer2/checkout/wallClockProfiler/testProgram/testProf]
  310,000,000          .       .  /build/buildd/eglibc-2.19/misc/../sysdeps/unix
/sysv/linux/llseek.c:llseek [/lib/i386-linux-gnu/libc-2.19.so]
  230,000,000          .       .  /build/buildd/eglibc-2.19/stdlib/random.c:rand
om [/lib/i386-linux-gnu/libc-2.19.so]
  220,000,008          .       .  testProf.cpp:readRandFileValue(_IO_FILE*) [/ho
me/jasonrohrer2/checkout/wallClockProfiler/testProgram/testProf]
  180,010,670          .       .  /build/buildd/eglibc-2.19/string/../sysdeps/i3
86/i686/multiarch/strcat.S:0x0012694b [/lib/i386-linux-gnu/libc-2.19.so]
  170,000,000          .       .  /build/buildd/eglibc-2.19/libio/fileops.c:_IO_
file_seek [/lib/i386-linux-gnu/libc-2.19.so]
  100,000,000          .       .  /build/buildd/eglibc-2.19/libio/libioP.h:fseek
  100,000,000          .       .  /build/buildd/eglibc-2.19/libio/libioP.h:getc
   99,995,670          .       .  /build/buildd/eglibc-2.19/io/../sysdeps/unix/s
yscall-template.S:__read_nocancel [/lib/i386-linux-gnu/libc-2.19.so]
   99,995,670          .       .  /build/buildd/eglibc-2.19/libio/fileops.c:_IO_
file_read [/lib/i386-linux-gnu/libc-2.19.so]
   90,000,048          .       .  testProf.cpp:main [/home/jasonrohrer2/checkout
/wallClockProfiler/testProgram/testProf]
   80,000,000          .       .  /build/buildd/eglibc-2.19/stdlib/rand.c:rand [
/lib/i386-linux-gnu/libc-2.19.so]
   39,999,154 19,999,578  50,323  ???:_dl_sysinfo_int80 [/lib/i386-linux-gnu/ld-
2.19.so]
```
Yes!  fseek is correctly at the top of the list.  But the program ran 11x slower (160 seconds vs 14 seconds).  Also, it's still overestimating the impact of the rand calls in the overall runtime.

For example, if we comment out the calls to fseek and fgetc, while leaving all the rand calls in place, the runtime drops from 14 seconds down to 0.25 seconds.  I.e., around 1% of the runtime is actually spent in rand, but the the Kcachegrind graphical visuallizer output (not shown here) pegs rand at 14% of the runtime, while only giving fseek 64%, instead of the 99% of the runtime that it actually occupies.

But at least valgrind finds fseek as the hotspot.
