#include <stdio.h>     
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include <limits.h>
#include <assert.h>


// Print out the usage of the program and exit.
void Usage(char*);
uint32_t jenkins_one_at_a_time_hash(const uint8_t*, size_t);
double GetTime();

//recursive multithreaded binary tree that will calculate the hash values
void* tree(void*);

#define BSIZE 4096 // block size

typedef struct thread_args {
	uint32_t nthreads; //the number of threads that the program will have
	uint32_t root_val; //the number of the thread in the tree 
	int32_t fd; //file descriptor
	uint32_t blocks_per_thread; // nblocks / nthreads
	uint8_t* addr; //the mmap return result
} args;

int main(int argc, char** argv) {
	args arg;
	uint32_t nblocks;
 
	// input checking 
	if (argc != 3) Usage(argv[0]);

	// open input file
	arg.fd = open(argv[1], O_RDWR);
	if (arg.fd == -1) {
		perror("open failed");
		exit(EXIT_FAILURE);
	}

	// use fstat to get file size
	struct stat buf;
	if (fstat(arg.fd, &buf) != 0){
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	off_t file_size = buf.st_size;
	long int height = strtol(argv[2], NULL, 10);
	//if underflow or overflow, height becomes LONG_MIN or LONG_MAX and sets errno 
	if (height == LONG_MAX || height == LONG_MIN){
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	//limit thread creation
	if (height < -1 || height > 11){
		printf("Invalid height\n");
		exit(EXIT_FAILURE);
	}

	arg.nthreads = 1 << (height+1);
	if (!arg.nthreads){
		printf("Could not get number of threads.\n");
		exit(EXIT_FAILURE);
	}
	//calculate nblocks 
	nblocks = file_size / BSIZE;
	arg.blocks_per_thread = nblocks / arg.nthreads; 
	if (!arg.blocks_per_thread){
		printf("Could not get blocks per thread.\n");
		exit(EXIT_FAILURE);
	}
	arg.addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, arg.fd, 0); //map file into addr field in struct

	printf("num Threads = %i \n", arg.nthreads);
	printf("Blocks per Thread = %u \n", arg.blocks_per_thread);


	pthread_t root; //root thread
	arg.root_val = 0;
	void* root_hash;
	uint32_t final_hash;
	//int i = 0;
	//for (; i < 5; i++){ //for loop used to gather data for report
	
	double start = GetTime();
	if (pthread_create(&root, NULL, tree, &arg) != 0){
		perror(NULL);
		exit(EXIT_FAILURE);
	} 
	if (pthread_join(root, &root_hash) != 0){
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	double end = GetTime();
	final_hash = *(uint32_t*)(root_hash);
	printf("hash value = %u\n", final_hash);
	printf("time taken = %f \n", (end - start));
	//}
	close(arg.fd);
	return EXIT_SUCCESS;
}

void* tree(void *arg){
	//arg is the number assigned to each thread. root thread is 0.
	//leftchild is 2*arg + 1, rightchild is 2*arg + 2
	args local_worker = *((args*)arg);

	pthread_t leftchild = 0, rightchild = 0; //0 is an invalid thread ID, the values will be changed through pthread_create()
	uint32_t leftval = 2*local_worker.root_val + 1, rightval = 2*local_worker.root_val + 2;
	args left, right;
	left.root_val = leftval;
	right.root_val = rightval;
	right.nthreads = left.nthreads = local_worker.nthreads;
	right.blocks_per_thread = left.blocks_per_thread = local_worker.blocks_per_thread;
	right.fd = left.fd = local_worker.fd;
	left.addr = right.addr = local_worker.addr; 

	if (leftval < local_worker.nthreads){
		if (pthread_create(&leftchild, NULL, tree, &left) != 0){
			perror(NULL);
			exit(EXIT_FAILURE);
		}
	}
	if (rightval < local_worker.nthreads){
		if (pthread_create(&rightchild, NULL, tree, &right) != 0){
			perror(NULL);
			exit(EXIT_FAILURE);
		}
	}
	//offset is the starting block offset 
	//len is how many blocks the thread will hash * block size
	off_t offset = local_worker.blocks_per_thread * local_worker.root_val * BSIZE;
	size_t len = (size_t)local_worker.blocks_per_thread * BSIZE; //not casting to size_t results in len = 0 at a file size of 4294967296

	uint32_t hash = jenkins_one_at_a_time_hash(local_worker.addr + offset, len);

	//wait for children to return values to join with the current local_worker
	void *left_returnval, *right_returnval;
	char hash_str[32];
	int hash_str_len = 0;
	uint32_t lefthash, righthash;
	uint32_t* hashval = (uint32_t*) malloc(sizeof(uint32_t)); //allocate to heap because the stack value gets lost upon exiting

	//leftchild and rightchild are both 0 if they have not been created, making these false
	if (leftchild){
		if (pthread_join(leftchild, &left_returnval) != 0){
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		lefthash = *(uint32_t*)left_returnval;  
	}
	if (rightchild){
		if (pthread_join(rightchild, &right_returnval) != 0){
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		righthash = *(uint32_t*)right_returnval;
	}
	//combine the results into one string or just return if combining is not needed
	if (leftchild && rightchild) { 
		hash_str_len = sprintf(hash_str, "%u%u%u", hash, lefthash, righthash);
		*hashval = jenkins_one_at_a_time_hash((uint8_t*)hash_str, hash_str_len);
		pthread_exit(hashval);
	}
	else if (leftchild) {
		hash_str_len = sprintf(hash_str, "%u%u", hash, lefthash);
		*hashval = jenkins_one_at_a_time_hash((uint8_t*)hash_str, hash_str_len);
		pthread_exit(hashval);
	}
	else *hashval = hash;
	return hashval;
}

uint32_t jenkins_one_at_a_time_hash(const uint8_t* key, size_t length) {
	size_t i = 0;
	uint32_t hash = 0;
	while (i != length) {
	hash += key[i++];
	hash += hash << 10;
	hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

void Usage(char* s) {
	fprintf(stderr, "Usage: %s filename height \n", s);
	exit(EXIT_FAILURE);
}

double GetTime() {
	struct timeval t;
	int rc = gettimeofday(&t, NULL);
	assert(rc == 0);
	return (double)t.tv_sec + (double)t.tv_usec/1e6;
}
