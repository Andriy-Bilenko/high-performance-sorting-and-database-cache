1. Write a program that sorts double precision numbers stored in a text file of size 1GB (one number per line). 
Example: 
* 8.33891e+307 
* 1.26192e+308 
* 
* 8.19572e+307 
* ... 
* 0 
1.64584e+304​
 
The program should use no more than 100MB of memory and run for no longer than 25-30 minutes (on a 
modern single-core processor with a clock speed of 2GHz).​
 
Required parameters: 
   <name of unsorted file>  <name of sorted file>​
 
Additionally, a generator for an unsorted 1GB file with double precision numbers must be written. 
 
-------------------------------------------------------------------------------------------------------------- 
 
---------------------------------------------------------------------------------------------------------------- 
​
2. Let's imagine that there is an interface to a database: 
 ```cpp
struct i_db 
{ 
    bool begin_transaction(); 
    bool commit_transaction(); 
    bool abort_transaction(); 
    std::string get(const std::string& key); 
    std::string set(const std::string& key, const std::string& data); 
    std::string delete(const std::string& key); 
} 
```
Write a cache implementation for the database,  
paying attention to multi-threading and the transactional model of working with the database. 