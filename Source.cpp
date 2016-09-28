#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include "fastPlayer.h"
#include "simplePlayer.h"

using namespace std;

int main() {
	int size, threads, turns;
	in >> size >> turns;// >> threads;

	vector< vector<int> > data(size, vector<int>(size, 0));
	for (size_t i = 0; i < size; i++) {
		for (size_t j = 0; j < size; j++) {
			in >> data[i][j];
		}
	}
	out << "smart method" << endl;

	for (size_t threads = 1; threads < 10; threads++) {
		if (size % threads == 0)
			game(size, turns, threads, data);
	}

	out << "simple method" << endl;

	for (size_t threads = 1; threads < 10; threads++) {
		if (size % threads == 0)
			simpleGame(size, turns, threads, data);
	}

	system("pause");
	return 0;
}