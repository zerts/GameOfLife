#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <omp.h>

using namespace std;

ifstream in("input");
ofstream out("output");

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
    vector< pair<size_t, size_t> > gN = getNeighbours(i, j, field.size());
    for (size_t kk = 0; kk < gN.size(); kk++) {
        aliveNeighbourCounter += field[gN[kk].first][gN[kk].second];
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
            vector< vector< vector<int> > > &field, vector<size_t> &progress) {
    for (size_t t = 1; t <= turns; t++) {
        for (size_t i = 0; i < field[0].size(); i++) {
            field[t % 2][top][i] = newCellValue(top, i, field[(t + 1) % 2]);
            field[t % 2][bottom - 1][i] = newCellValue(bottom - 1, i, field[(t + 1) % 2]);
        }
        progress[id]++;
        cv.notify_all();
        for (size_t i = top + 1; i < bottom - 1; i++) {
            for (size_t j = 0; j < field[0].size(); j++) {
                field[t % 2][i][j] = newCellValue(i, j, field[(t + 1) % 2]);
            }
        }
        while (progress[(id + 1) % progress.size()] < t
               || progress[(id - 1 + progress.size()) % progress.size()] < t) {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck);
        }
    }
}

void game(size_t size, size_t turns, int threads,
          vector< vector <int> > &data) {
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

    size_t width = (size + ((size % threads) != 0)) / threads;
    vector< vector< vector<int> > > field(2, vector< vector<int> >(size, vector<int>(size, 0)));
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < size; j++) {
            field[0][i][j] = data[i][j];
        }
    }
    vector<thread> workers;
    vector<size_t> progress((unsigned long) threads, 0);

#pragma omp parallel num_threads(threads) shared(field, progress)
    {
        size_t id = (size_t) omp_get_thread_num();
        size_t top = id * width;
        size_t bottom = min(size, (id + 1u) * width);
        for (size_t t = 1; t <= turns; t++) {
            for (size_t i = 0; i < field[0].size(); i++) {
                field[t % 2][top][i] = newCellValue(top, i, field[(t + 1) % 2]);
                field[t % 2][bottom - 1][i] = newCellValue(bottom - 1, i, field[(t + 1) % 2]);
            }
            progress[id]++;
            cv.notify_all();
            for (size_t i = top + 1; i < bottom - 1; i++) {
                for (size_t j = 0; j < field[0].size(); j++) {
                    field[t % 2][i][j] = newCellValue(i, j, field[(t + 1) % 2]);
                }
            }
            while (progress[(id + 1) % progress.size()] < t
                   || progress[(id - 1 + progress.size()) % progress.size()] < t) {
                std::unique_lock<std::mutex> lck(mtx);
                cv.wait(lck);
            }
        }
    }

    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "time with " << threads << "threads = " << elapsed_seconds.count() << std::endl;
}

int main(int argc, char **argv) {
    int size, turns;
    size = 2 * 2 * 2 * 2 * 2 * 3 * 3 * 5;
    turns = 5;
    vector< vector<int> > data((unsigned long) size, vector<int>((unsigned long) size, 0));
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < size; j++) {
            data[i][j] = (rand() > 0.5 ? 1 : 0);
        }
    }
    out << "smart method" << endl;

    for (int threads = 1; threads < size; threads++) {
        if (size % threads == 0)
            game((size_t) size, (size_t) turns, threads, data);
    }

    return 0;
}