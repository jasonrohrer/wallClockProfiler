A simple I/O bound test program.

Build like this:

g++ -o makeTestFile makeTestFile.cpp
g++ -o testProf testProf.cpp


First run:

./makeTestFile

This will make a 200 MB file filled with random bytes (testFile.bin).


The I/O bound test program, which seeks and reads in this test file, can then
be called like this:

./testProf
