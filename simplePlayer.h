#pragma once
#include "fastPlayer.h"

void simpleWorker(size_t id, size_t turns, size_t top, size_t bottom,
	vector< vector<int> > &field, vector<size_t> &progress) {
	for (size_t i = top; i < bottom; i++) {
		for (size_t j = 0; j < field.size(); j++) {
			field[i][j] = newCellValue(i, j, field);
		}
	}
}

void simpleGame(size_t size, size_t turns, size_t threads,
	vector< vector <int> > &data) {
	std::chrono::time_point<std::chrono::system_clock> start, end;
	start = std::chrono::system_clock::now();

	size_t width = (size + ((size % threads) != 0)) / threads;
	vector< vector<int> > result(size, vector<int>(size, 0));
	for (size_t i = 0; i < size; i++) {
		for (size_t j = 0; j < size; j++) {
			result[i][j] = data[i][j];
		}
	}
	while (turns > 0) {
		vector<thread> workers;
		vector<size_t> progress(threads, 0);

		vector<vector<int> > field(size, vector<int>(size, 0));
		for (size_t i = 0; i < size; i++) {
			for (size_t j = 0; j < size; j++) {
				field[i][j] = result[i][j];
			}
		}

		for (size_t i = 0; i < threads; i++) {
			workers.push_back(thread(simpleWorker, i, turns, i * width, min(size, (i + 1u) * width), ref(field), ref(progress)));
		}
		for (size_t i = 0; i < threads; i++) {
			workers[i].join();
		}

		for (size_t i = 0; i < size; i++) {
			for (size_t j = 0; j < size; j++) {
				result[i][j] = field[i][j];
			}
		}

		turns--;
		if (turns == 0) {
			for (size_t i = 0; i < size; i++) {
				for (size_t j = 0; j < size; j++) {
					cout << result[i][j] << ' ';
				}
				cout << endl;
			}
		}
	}

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	out << "time with " << threads << "threads = " << elapsed_seconds.count() << std::endl;
}