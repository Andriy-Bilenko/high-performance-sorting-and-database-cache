#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

const size_t CHUNK_SIZE =
	90 * 1024 *
	1024;  // 90 MB (hardcoded value to keep memory usage under 100 MB)

void sort_and_save_chunk(std::ifstream &input,
						 const std::string &temp_filename) {
	std::vector<double> numbers;
	double number;

	// Read numbers into memory
	size_t max_numbers_count = CHUNK_SIZE / sizeof(double);
	numbers.reserve(max_numbers_count);
	while (input && numbers.size() < max_numbers_count) {
		input >> number;
		if (input) {
			numbers.push_back(number);
		}
	}

	// Sort the numbers
	std::sort(numbers.begin(), numbers.end());

	// Save sorted numbers to a temporary file
	std::ofstream temp_file(temp_filename);
	for (double num : numbers) {
		temp_file << std::scientific
				  << std::setprecision(
						 std::numeric_limits<double>::max_digits10)
				  << num << "\n";
	}
	temp_file.close();
}

void merge_sorted_files(const std::vector<std::string> &temp_filenames,
						const std::string &output_filename) {
	auto cmp = [](const std::pair<double, size_t> &a,
				  const std::pair<double, size_t> &b) {
		return a.first > b.first;
	};

	std::priority_queue<std::pair<double, size_t>,	// <num, file_index>
						std::vector<std::pair<double, size_t>>, decltype(cmp)>
		min_heap(cmp);
	std::vector<std::ifstream> temp_files;

	// Open temporary files
	for (const auto &filename : temp_filenames) {
		temp_files.emplace_back(filename);
		double num;
		if (temp_files.back() >> num) {
			min_heap.emplace(num, temp_files.size() - 1);
		}
	}

	std::ofstream output_file(output_filename);
	while (!min_heap.empty()) {
		auto [num, index] = min_heap.top();
		min_heap.pop();
		output_file << std::scientific
					<< std::setprecision(
						   std::numeric_limits<double>::max_digits10)
					<< num << "\n";

		// Read next number from the same file
		double next_num;
		if (temp_files[index] >> next_num) {
			min_heap.emplace(next_num, index);
		}
	}

	output_file.close();
}

void sort_large_file(const std::string &input_filename,
					 const std::string &output_filename) {
	std::ifstream input_file(input_filename);
	if (!input_file) {
		std::cerr << "Error opening input file.\n";
		return;
	}

	std::vector<std::string> temp_filenames;
	std::string temp_filename;

	// Sort and save chunks
	while (input_file.peek() != EOF) {
		temp_filename =
			"temp_" + std::to_string(temp_filenames.size()) + ".txt";
		sort_and_save_chunk(input_file, temp_filename);
		temp_filenames.push_back(temp_filename);
	}

	input_file.close();

	// Merge sorted files
	merge_sorted_files(temp_filenames, output_filename);

	// delete tmp files
	for (const auto &filename : temp_filenames) {
		if (std::remove(filename.c_str()) == 0) {
			std::cout << "Successfully deleted tmp file: " << filename
					  << std::endl;
		} else {
			std::cout << "Failed to delete tmp file: " << filename << std::endl;
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <input file> <output file>\n";
		return 1;
	}

	auto start_time = std::chrono::high_resolution_clock::now();
	sort_large_file(argv[1], argv[2]);
	auto end_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_time = end_time - start_time;

	std::cout << "Sorting completed successfully in " << elapsed_time.count()
			  << " seconds.\n";
	return 0;
}
