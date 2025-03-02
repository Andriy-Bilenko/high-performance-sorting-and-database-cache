#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <random>

void generate_1gig_file(std::string filename) {
	const size_t TARGET_SIZE = 1L * 1024 * 1024 * 1024;	 // 1GB
	std::ofstream out(filename, std::ios::trunc);
	if (!out) {
		std::cerr << "Error opening file for writing!\n";
		return;
	}

	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_real_distribution<double> dist(1.0, 1.0e308);
	int precision = std::numeric_limits<double>::max_digits10;

	auto start_time = std::chrono::high_resolution_clock::now();

	size_t file_size = 0;
	std::ostringstream oss;
	while (file_size < TARGET_SIZE) {
		double num = dist(gen);
		oss << std::scientific << std::setprecision(precision) << num << "\n";
		out << oss.str();
		file_size += oss.str().size();
		oss.str("");  // Clear the contents
		oss.clear();  // Clear any error flags
		out.flush();  // Ensure data is written
	}

	out.close();

	auto end_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_time = end_time - start_time;

	std::cout << "File '" << filename << "' generated successfully ("
			  << file_size / (1024 * 1024) << " MB) in " << elapsed_time.count()
			  << " seconds.\n";
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <input file>\n";
		return 1;
	}
	std::string output_filename = argv[1];

	generate_1gig_file(output_filename);

	return 0;
}
