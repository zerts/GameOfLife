#pragma once
#include <vector>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

ifstream in("input.txt");
ofstream out("output.txt");

mutex mtx;
condition_variable cv;

vector< pair<size_t, size_t> > getNeighbours(size_t i, size_t j, size_t size) {
	vector< pair<size_t, size_t> > result;
	size_t 
		up = (i + size - 1) % size, 
		down = (i + 1) % size, 
		left = (j + size - 1) % size, 
		right = (j + 1) % size;
	result.push_back(make_pair(up, left));
	result.push_back(make_pair(up, j));
	result.push_back(make_pair(up, right));
	result.push_back(make_pair(i, left));
	result.push_back(make_pair(i, right));
	result.push_back(make_pair(down, left));
	result.push_back(make_pair(down, j));
	result.push_back(make_pair(down, right));
	return result;
}

int neighbourCounter(size_t i, size_t j, vector< vector<int> > &field) {
	int aliveNeighbourCounter = 0;
	for (auto &k : getNeighbours(i, j, field.size())) {
		aliveNeighbourCounter += field[k.first][k.second];
	}
	return aliveNeighbourCounter;
}

bool surviveTheTurn(size_t i, size_t j, vector< vector<int> > &field) {
	return (field[i][j] && (neighbourCounter(i, j, field) == 2 || neighbourCounter(i, j, field) == 3));
}

bool becomeAlive(size_t i, size_t j, vector< vector<int> > &field) {
	return (!field[i][j] && neighbourCounter(i, j, field) == 3);
}

bool becomeDead(size_t i, size_t j, vector< vector<int> > &field) {
	return (field[i][j] && (neighbourCounter(i, j, field) < 2 || neighbourCounter(i, j, field) == 4));
}

enum Condition {
	SURVIVE,
	AWAKE,
	DIE,
	DESERT
};

Condition detectCondition(size_t i, size_t j, vector< vector<int> > &field) {
	if (surviveTheTurn(i, j, field))
		return Condition::SURVIVE;
	if (becomeAlive(i, j, field))
		return Condition::AWAKE;
	if (becomeDead(i, j, field))
		return Condition::DIE;
	return Condition::DESERT;
}

int newCellValue(size_t i, size_t j, vector< vector<int> > &field) {
	return (detectCondition(i, j, field) == Condition::AWAKE
		|| detectCondition(i, j, field) == Condition::SURVIVE ? 1 : 0);
}

void worker(size_t id, size_t turns, size_t top, size_t bottom,
	vector< vector< vector<int> > > &field, vector<size_t> &progress, 
	size_t depth) {
	for (size_t k = 1; k <= depth; k++) {
		for (size_t i = top + k; i < bottom - k; i++) {
			for (size_t j = 0; j < field[0].size(); j++) {
				field[k][i][j] = newCellValue(i, j, field[k - 1]);
			}
		}
		progress[id]++;
		cv.notify_all();
	}

	for (size_t level = 1; level <= depth; level++) {
		while (progress[(id + 1) % progress.size()] < level
			|| progress[(id - 1 + progress.size()) % progress.size()] < level) {
			std::unique_lock<std::mutex> lck(mtx);
			cv.wait(lck);
		}
		for (size_t l = level; l <= depth; l++) {
			for (size_t j = 0; j < field[0].size(); j++) {
				field[l][top + (l - level)][j] = newCellValue(top + (l - level), j, field[l - 1]);
				field[l][bottom - 1 - (l - level)][j] = newCellValue(bottom - 1 - (l - level), j, field[l - 1]);
			}
		}
		progress[id]++;
		cv.notify_all();
	}
}

void game(size_t size, size_t turns, size_t threads, 
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
		size_t depth = min(width / 2, turns);
		vector<thread> workers;
		vector<size_t> progress(threads, 0);

		vector< vector< vector<int> > > field(depth + 1, vector< vector <int> >(size, vector<int>(size, 0)));
		for (size_t i = 0; i < size; i++) {
			for (size_t j = 0; j < size; j++) {
				field[0][i][j] = result[i][j];
			}
		}

		for (size_t i = 0; i < threads; i++) {
			workers.push_back(thread(worker, i, turns, i * width, min(size, (i + 1u) * width), ref(field), ref(progress), depth));
		}
		for (size_t i = 0; i < threads; i++) {
			workers[i].join();
		}

		for (size_t i = 0; i < size; i++) {
			for (size_t j = 0; j < size; j++) {
				result[i][j] = field[depth][i][j];
			}
		}

		turns -= depth;
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