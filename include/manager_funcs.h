/* File: manager_funcs.h */

#include <string>
#include <queue>
#include <map>

/* Attempts to create a worker and give it new_file to process, updating all structures as necessary. */
/* Returns 0 on success, -1 on failure. If manager is fine but worker failed, it is considered a success. */
int make_worker(std::string &new_file, const char* pipe_dir, std::map<int,int> &worker_to_pipe, int &num_alive_workers, sigset_t block_set);

/* Wait without blocking for all children that have changed state, updating the queue of available workers and the number of alive workers. */
/* Returns 0 on success, -1 on failure. */
int gather_children(std::queue<int> &available_workers, int &num_alive_workers, int listen_pid);