/* File: worker_funcs.h */

#include <string>
#include <map>

/* Size of the buffer for reading the file */
#define BUFFER_SIZE 100

/* Function that finds the number of appearences of each URL in file_name and puts the result in url_nums */
/* Returns 0 on success, -1 on failure */
int extract_urls(std::map<std::string,int> &url_nums, const char* file_name);

/* Permissions to give to the new file */
#define PERMS 0644

/* Function that outputs the number of appearences of each URL in url_nums to file_name */
/* Returns 0 on success, -1 on failure */
int write_urls(const std::map<std::string,int> &url_nums, const char* file_name);