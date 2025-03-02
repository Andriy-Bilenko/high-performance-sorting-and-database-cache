#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Abstract structure for a database interface
struct i_db {
	virtual bool begin_transaction() = 0;
	virtual bool commit_transaction() = 0;
	virtual bool abort_transaction() = 0;
	virtual std::string get_key(const std::string& key) = 0;
	virtual std::string set_key(const std::string& key,
								const std::string& data) = 0;
	virtual std::string delete_key(const std::string& key) = 0;
};

// recently used cache for key-value pairs.
// contains list of key-value pairs and a hashmap of keys to iterators
// (pointers) to list elements, for O(1) access. No iterators are invalidated in
// the process.
// when get() or put() gets called moves accessed key to the front of the cache.
// caches delete calls using put() with std::nullopt for value parameter
struct Cache {
   private:
	// max size of key-value pairs to cache
	size_t m_capacity;
	// actual cache as a list.
	// if optional is std::nullopt the key-value pair is deleted
	std::list<std::pair<std::string, std::optional<std::string>>> m_cache;
	// map<key, pointer(iterator) to list element> for fast access
	std::unordered_map<
		std::string,
		std::list<std::pair<std::string, std::optional<std::string>>>::iterator>
		m_cache_map;

	// moves key-value pair in a list to front of the cache in O(1)
	void move_to_front(const std::string& key) {
		auto it = m_cache_map[key];
		m_cache.splice(m_cache.begin(), m_cache, it);
	}

   public:
	Cache(size_t capacity) : m_capacity(capacity) {}

	// Puts a pair to the cache (you can also put deletion with std::nullopt)
	// if pair is already in cache - moves it to the front
	// else pushes this new key-value pair to the front of the cache
	// if buffer full - removes 1 item at back of cache
	void put(const std::string& key, const std::optional<std::string>& value) {
		if (m_cache_map.find(key) != m_cache_map.end()) {
			// key is found in a map
			m_cache_map[key]->second = value;
			move_to_front(key);
			return;
		}

		if (m_cache.size() >= m_capacity) {
			// key is not in map and the capacity is used up
			m_cache_map.erase(m_cache.back().first);
			m_cache.pop_back();
		}

		// key is not in a map so we add it to the front
		m_cache.emplace_front(key, value);
		m_cache_map[key] = m_cache.begin();
	}

	// Gets value item from the cache given key
	// if there's no key in cache - returns false
	// else returns true, returns actual std::optional<std::string> value
	// through value parameter if it exists in cache and moves key-value pair to
	// the front of cache
	bool get(const std::string& key, std::optional<std::string>& value) {
		auto it = m_cache_map.find(key);
		if (it == m_cache_map.end()) return false;

		value = it->second->second;
		// recently used, so we put to the front of the cache
		move_to_front(key);
		return true;
	}

	// Prints cache capacity and the cache contents from front to back
	void print_self() const {
		std::cout << "cache capacity - " << m_capacity
				  << " key-value pairs\r\n";
		for (const auto& [key, value_opt] : m_cache) {
			std::cout << key << ": ";
			if (value_opt.has_value()) {
				std::cout << value_opt.value();
			} else {
				std::cout << "<deleted>";
			}
			std::cout << "\r\n";
		}
	}
};

// Example database interface implementation with optional caching and thread
// safety conforming to ACID.
// Uses simple .txt file as a database by storing "{key}={value}" one per line,
// file operations are not optimised (it's better to use real databases like
// PostgreSQL for that).
// Each thread has its own thread local transaction data (uncommited changes),
// but all threads access one file and one cache synchronized using mutexes.
class CachedFileDatabase : public i_db {
   private:
	std::optional<Cache> m_local_cache;
	std::mutex m_local_cache_mutex;
	std::string m_filename;
	std::mutex m_file_mutex;

	// Thread-locals
	thread_local static bool ts_transaction_active;
	thread_local static std::unordered_map<std::string, std::string>
		ts_transaction_data;
	thread_local static std::unordered_set<std::string> ts_transaction_deletes;

	// Helper function to write or delete a key-value pair in the file
	// not thread-safe
	void file_write_or_delete(bool is_delete, const std::string& key,
							  const std::string& value) {
		std::ifstream fin(m_filename);
		if (!fin) {
			std::cerr << "Error opening file for reading!\n";
			return;
		}

		std::vector<std::string> lines;
		std::string line;
		bool is_key_found{false};

		// looping through file reading it all into a vector with modification
		while (std::getline(fin, line)) {
			if (!is_key_found && line.find(key + "=") == 0) {
				is_key_found = true;
				if (is_delete) {
					// Skip line to delete it
				} else {
					lines.push_back(key + "=" + value);
				}
				continue;
			}
			lines.push_back(line);
		}
		fin.close();
		if (!is_key_found) {
			if (is_delete) {
				return;	 // nothing to delete
			} else {
				lines.push_back(key + "=" + value);
			}
		}

		// Write back modified content
		std::ofstream fout(m_filename);
		if (!fout) {
			std::cerr << "Error opening file for writing!\n";
			return;
		}

		for (const auto& line : lines) {
			fout << line << '\n';
		}
		fout.close();
	}

	// writes new or modifies existing key-value pair in a file
	// not thread-safe
	void file_write_key_value(const std::string& key,
							  const std::string& value) {
		file_write_or_delete(false, key, value);
	}

	// deletes line with key-value pair in a file if it exists
	// not thread-safe
	void file_delete_key(const std::string& key) {
		file_write_or_delete(true, key, "");
	}

	// gets value from key-value pairs in a file if it exists, "" otherwise or
	// in case of a file error
	// not thread-safe
	std::string file_get_value(const std::string& key) {
		std::ifstream fin(m_filename);
		if (!fin) {
			std::cerr << "Error opening file for reading!\n";
			return "";
		}
		std::string line;
		while (std::getline(fin, line)) {
			if (line.find(key + "=") ==
				0) {  // "{key}=" at the start of a string
				size_t pos = line.find('=');
				if (pos != std::string::npos) {
					return line.substr(pos + 1);
				}
				break;
			}
		}
		fin.close();
		// if no key found return empty string
		return "";
	}

   public:
	CachedFileDatabase(const std::string& file, int cache_size = 0)
		: m_filename(file) {
		// we have cache only if we set its size properly. no cache by default
		if (cache_size > 0) {
			m_local_cache = Cache(cache_size);
		}
	}

	// begins new DB transaction
	// returns false if is already in a transaction
	// otherwise returns true and cleans thread_local variables
	virtual bool begin_transaction() override {
		if (ts_transaction_active) return false;  // Already in a transaction
		ts_transaction_active = true;
		ts_transaction_data.clear();
		ts_transaction_deletes.clear();
		return true;
	}

	// returns false if no transaction is active
	// otherwise finalizes transaction to DB file and local cache from
	// thread_local variables containing uncommited changes and returns true
	virtual bool commit_transaction() override {
		if (!ts_transaction_active) return false;

		{
			// locking everything (transacions should appear atomic)
			std::lock_guard<std::mutex> file_lock(m_file_mutex);
			std::lock_guard<std::mutex> local_cache_lock(m_local_cache_mutex);

			// 1. write to file set_key operations
			for (const auto& pair : ts_transaction_data) {
				file_write_key_value(pair.first, pair.second);
				// write to cache if it exists
				if (m_local_cache.has_value()) {
					m_local_cache->put(pair.first, pair.second);
				}
			}

			// 2. write to file delete_key operations
			for (const std::string& key : ts_transaction_deletes) {
				file_delete_key(key);
				// write to cache if it exists
				if (m_local_cache.has_value()) {
					m_local_cache->put(key, std::nullopt);
				}
			}
		}
		// Clear transaction state
		ts_transaction_data.clear();
		ts_transaction_deletes.clear();
		ts_transaction_active = false;

		return true;
	}

	// aborts current uncommited changes
	// returns false if no changes to abort, otherwise true
	virtual bool abort_transaction() override {
		if (!ts_transaction_active) return false;
		ts_transaction_data.clear();
		ts_transaction_deletes.clear();
		ts_transaction_active = false;
		return true;
	}

	// gets value given key
	// first looks for data in uncommited changes, then in local cache, at last
	// reads from DB file (and does additional caching).
	// returns "" if nothing was found / transaction was not started
	// returns value otherwise
	virtual std::string get_key(const std::string& key) override {
		if (!ts_transaction_active) {
			// you did not start the transaction!
			return "";
		} else {
			// 1. check in current transaction uncommited changes
			if (ts_transaction_deletes.find(key) !=
				ts_transaction_deletes.end()) {
				return "";	// Key marked for deletion in transaction
			}
			if (ts_transaction_data.find(key) != ts_transaction_data.end()) {
				return ts_transaction_data[key];  // Return uncommitted value
			}
			// 2. check in cache
			{
				std::lock_guard<std::mutex> local_cache_lock(
					m_local_cache_mutex);
				// if local cache exists
				if (m_local_cache.has_value()) {
					std::optional<std::string> tmp_value;
					bool exists = m_local_cache->get(key, tmp_value);
					if (exists) {
						return tmp_value.value_or("");
					}
				}
			}

			// 3. get from actual file
			std::string value{""};
			{
				std::lock_guard<std::mutex> file_lock(m_file_mutex);
				value = file_get_value(key);
			}
			// putting to the local cache if it exists:
			{
				std::lock_guard<std::mutex> local_cache_lock(
					m_local_cache_mutex);
				if (m_local_cache.has_value()) {
					m_local_cache->put(key, std::nullopt);
				}
			}
			return value;
		}
	}

	// adds new key-value pair (or modifies existing) to uncommited changes
	// returns previous value at that key if it exists
	// returns "" if it doesn't exist or transaction wasn't started
	virtual std::string set_key(const std::string& key,
								const std::string& data) override {
		if (!ts_transaction_active) {
			// you did not start the transaction!
			return "";
		} else {
			std::string old_value = get_key(key);
			ts_transaction_data[key] = data;
			ts_transaction_deletes.erase(key);
			return old_value;
		}
	}

	// adds delete key query to uncommited changes
	// returns previous value at that key if it exists
	// returns "" if it doesn't exist or transaction wasn't started
	virtual std::string delete_key(const std::string& key) override {
		if (!ts_transaction_active) {
			// you did not start the transaction!
			return "";
		} else {
			std::string old_value = get_key(key);
			ts_transaction_data.erase(key);
			ts_transaction_deletes.insert(key);
			return old_value;
		}
	}

	// functions for debugging/testing
	void print_cache() {
		std::lock_guard<std::mutex> local_cache_lock(m_local_cache_mutex);
		if (!m_local_cache.has_value()) {
			std::cout << "no cache.\r\n";
		} else {
			m_local_cache->print_self();
		}
	}
	void print_uncommited() {
		std::cout << "transaction_data: \r\n";
		for (const auto& pair : ts_transaction_data) {
			std::cout << pair.first << ": " << pair.second << "\r\n";
		}
		std::cout << "transaction_deletes: \r\n";
		for (const std::string& key : ts_transaction_deletes) {
			std::cout << key << "\r\n";
		}
	}
};

thread_local bool CachedFileDatabase::ts_transaction_active = false;
thread_local std::unordered_map<std::string, std::string>
	CachedFileDatabase::ts_transaction_data;
thread_local std::unordered_set<std::string>
	CachedFileDatabase::ts_transaction_deletes;

// mutex for testing in test1(), for cleaner console output
std::mutex console_print_mutex;

void test1(int thread_id, CachedFileDatabase& db) {
	if (!db.begin_transaction()) {
		std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
		std::cerr << "Thread " << thread_id << ": Failed to begin transaction!"
				  << std::endl;
		return;
	}

	std::string key1 = "key" + std::to_string(thread_id) + "_1";
	std::string key2 = "key" + std::to_string(thread_id) + "_2";
	std::string value1 = "value" + std::to_string(thread_id) + "_1";
	std::string value2 = "value" + std::to_string(thread_id) + "_2";

	db.set_key(key1, value1);
	db.set_key(key2, value2);
	{
		std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
		std::cout << "Thread " << thread_id << ": Set " << key1 << " = "
				  << value1 << std::endl;
		std::cout << "Thread " << thread_id << ": Set " << key2 << " = "
				  << value2 << std::endl;
	}

	std::string result1 = db.get_key(key1);
	std::string result2 = db.get_key(key2);
	{
		std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
		std::cout << "Thread " << thread_id << ": Got " << key1 << " = "
				  << result1 << std::endl;
		std::cout << "Thread " << thread_id << ": Got " << key2 << " = "
				  << result2 << std::endl;
	}

	db.delete_key(key1);
	{
		std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
		std::cout << "Thread " << thread_id << ": Deleted " << key1
				  << std::endl;
	}

	if (!db.commit_transaction()) {
		{
			std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
			std::cerr << "Thread " << thread_id
					  << ": Failed to commit transaction!" << std::endl;
		}
		return;
	} else {
		std::lock_guard<std::mutex> console_print_lock(console_print_mutex);
		std::cerr << "Thread " << thread_id << ": Committed transaction!"
				  << std::endl;
	}

	db.print_cache();
}

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Usage: " << argv[0]
				  << " <input file> <max num of cache elements> <num of "
					 "threads>\r\n";
		return 1;
	}
	int max_cache_elements{};
	int number_of_threads{};
	try {
		max_cache_elements = std::stoi(argv[2]);
		number_of_threads = std::stoi(argv[3]);
	} catch (const std::exception& e) {
		std::cerr << "Error: Invalid number as an argument!\r\n";
		return 1;
	}

	CachedFileDatabase db(argv[1], max_cache_elements);

	std::vector<std::thread> threads;

	for (int i = 0; i < number_of_threads; ++i) {
		threads.push_back(std::thread(test1, i, std::ref(db)));
	}

	for (auto& t : threads) {
		t.join();
	}

	std::cout << "Final cache:" << std::endl;
	db.print_cache();

	return 0;
}
