To get started with this project, you will need a Linux system or a Mac. This is because the project includes headers only available in linux and mac systems, located in '/usr/include/sys/' (although not visible in MacOS).

To compile the project, simply clone the repo and compile src/htree.c with gcc or clang using no additional options.

After compiling the project, run the binary output with ./\<output_name\> \<filename\> \<height\>

Test cases are currently not available here. This is because the test cases provided by my professor were too large for GitHub, even after compression. However, results from running the tests have been provided in the test_results_archive directory. The file sizes used were very specific, using files exactly of size 256 MiB, 512 MiB, 1 GiB, 2GiB, and 4GiB. I am currently looking into git-LFS to store these large files. Currently, the project is __NOT__ replicable as originally used unless any user cloning this repo decides to create their own test data.

For now, I have included the results as well as the data collected from running the program 30+ times to ensure statistical significance. There was a small error in collecting the data, because the first iteration in a loop would always take longer than the next iterations, which was included in the calculations. This was a mistake. 

Also note that the results only include running the program using 1, 16, 64, 256, 1024, and 4096 threads, but any height specified between -1 and 11 should work.

The threads are designed in a way which act like a binary tree, where each thread will usually create two children threads, depending on the height specified initially. Each thread will then create their own hash and pass it to the parent to be rehashed with the parents value, the value of the left child, and the value of the right child, if there is one. The resulting hash value will differ based on the height of the tree.