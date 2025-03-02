#include <fstream>
#include <iostream>
#include <string>

bool is_sorted(const std::string &filename) {
	std::ifstream file(filename);
	if (!file) {
		std::cerr << "Error opening file: " << filename << "\n";
		return false;
	}

	double previous_number;
	bool first_number = true;

	std::string line;
	while (std::getline(file, line)) {
		double current_number;
		try {
			current_number = std::stod(line);
		} catch (const std::invalid_argument &) {
			std::cerr << "Invalid number format: " << line << "\n";
			return false;
		}

		// If it's the first number, just store it
		if (first_number) {
			previous_number = current_number;
			first_number = false;
			continue;
		}

		if (current_number < previous_number) {
			std::cout << "File is NOT sorted: " << previous_number << " > "
					  << current_number << "\n";
			return false;
		}

		previous_number = current_number;
	}

	return true;
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <input file>\n";
		return 1;
	}

	if (is_sorted(argv[1])) {
		std::cout << "The sorted file is in correct order.\n";
	} else {
		std::cout << "The sorted file has issues.\n";
	}

	return 0;
}
