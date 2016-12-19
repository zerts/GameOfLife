#ifndef GAMEOFLIFEMPI_FASTPLAYERMPI_H
#define GAMEOFLIFEMPI_FASTPLAYERMPI_H

#pragma once
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <mpi/mpi.h>
#include <stdlib.h>
#include <malloc.h>

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
    for (auto &k : getNeighbours(i, j, field.size())) {
        aliveNeighbourCounter += field[k.first][k.second];
    }
    return aliveNeighbourCounter;
}

bool surviveTheTurn(int neighbourCounter, int curr) {
    return (curr && (neighbourCounter == 2 || neighbourCounter == 3));
}

bool becomeAlive(int neighbourCounter, int curr) {
    return (!curr && neighbourCounter == 3);
}

bool becomeDead(int neighbourCounter, int curr) {
    return (curr && (neighbourCounter < 2 || neighbourCounter == 4));
}

enum Condition {
    SURVIVE,
    AWAKE,
    DIE,
    DESERT
};

Condition detectCondition(int neighbourCounter, int curr) {
    if (surviveTheTurn(neighbourCounter, curr))
        return Condition::SURVIVE;
    if (becomeAlive(neighbourCounter, curr))
        return Condition::AWAKE;
    if (becomeDead(neighbourCounter, curr))
        return Condition::DIE;
    return Condition::DESERT;
}

int newCellValue(int neighbourCounter, int curr) {
    return (detectCondition(neighbourCounter, curr) == Condition::AWAKE
            || detectCondition(neighbourCounter, curr) == Condition::SURVIVE ? 1 : 0);
}

class Worker{
    size_t width;
    int curr_number, l_number, r_number;
    int *fullField, *field[2], *l_neigh, *r_neigh, *sizes, *starts;


public:
    int number_of_workers;

    void readAllField(size_t size) {
        fullField = (int *) malloc(size * size * sizeof(int));
        sizes = (int *) malloc(number_of_workers * sizeof(int));
        starts = (int *) malloc(number_of_workers * sizeof(int));
        for (int i = 0; i < size * size; i++) {
            int newCell;
            in >> newCell;
            fullField[i] = newCell;
        }
        for (int i = 0; i < number_of_workers; i++) {
            sizes[i] = (int) (size * width);
            starts[i] = i * sizes[i];
        }
    }

    Worker(int argc, char **argv, size_t size) {
        MPI_Init(&argc, &argv);
        MPI_Comm_size(MPI_COMM_WORLD, &number_of_workers);
        MPI_Comm_rank(MPI_COMM_WORLD, &curr_number);

        l_number = (number_of_workers + curr_number - 1) % number_of_workers;
        r_number = (curr_number + 1) % number_of_workers;

        width = (size + ((size % number_of_workers) != 0)) / number_of_workers;

        field[0] = (int *) malloc(size * width * sizeof(int));
        field[1] = (int *) malloc(size * width * sizeof(int));
        l_neigh = (int *) malloc(size * sizeof(int));
        r_neigh = (int *) malloc(size * sizeof(int));

        cout << number_of_workers << ' ' << width << "reading started\n";

        if (curr_number == 0) {
            readAllField(size);
        }

        cout << "reading finished\n";

        MPI_Scatterv(fullField, sizes, starts, MPI_INT, field[0], (int) (size * width), MPI_INT, 0, MPI_COMM_WORLD);

        for (int i = 0; i < size * width; i++) {
            cout << field[0][i] << ' ';
        }
        cout << endl;

        cout << "scattering finished\n";

        MPI_Status status;
        MPI_Send(field[0], (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD);

        cout << "sending in process\n";

        MPI_Send(field[0] + (size * (width - 1)), (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD);

        cout << "sending finished\n";

        MPI_Recv(l_neigh, (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(r_neigh, (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &status);
    }

    void play(size_t size, size_t turns) {
        int neighbourCounter;
        for (size_t t = 1; t <= turns; t++) {
            for (size_t i = 0; i < size; i++) {

                neighbourCounter = l_neigh[i] + field[t % 2][size + i];
                if (i > 0) {
                    neighbourCounter += l_neigh[i - 1]
                                        + field[t % 2][size + i - 1]
                                        + field[t % 2][i - 1];
                }
                else {
                    neighbourCounter += l_neigh[size - 1]
                                        + field[t % 2][2 * size - 1]
                                        + field[t % 2][size - 1];
                }
                if (i < size - 1) {
                    neighbourCounter += l_neigh[i + 1]
                                        + field[t % 2][size + i + 1]
                                        + field[t % 2][i + 1];
                }
                else {
                    neighbourCounter += l_neigh[0]
                                        + field[t % 2][size]
                                        + field[t % 2][0];
                }

                field[t % 2][i] = newCellValue(neighbourCounter, field[(t + 1) % 2][i]);





                neighbourCounter = r_neigh[i] + field[t % 2][(width - 2) * size + i];
                if (i > 0) {
                    neighbourCounter += r_neigh[i - 1]
                                        + field[t % 2][(width - 2) * size + i - 1]
                                        + field[t % 2][(width - 1) * size + i - 1];
                }
                else {
                    neighbourCounter += r_neigh[size - 1]
                                        + field[t % 2][(width - 2) * size + size - 1]
                                        + field[t % 2][(width - 1) * size + size - 1];
                }
                if (i < size - 1) {
                    neighbourCounter += r_neigh[i + 1]
                                        + field[t % 2][(width - 2) * size + i + 1]
                                        + field[t % 2][(width - 1) * size + i + 1];
                }
                else {
                    neighbourCounter += r_neigh[0]
                                        + field[t % 2][(width - 2) * size]
                                        + field[t % 2][(width - 1) * size];
                }

                field[t % 2][i + (width - 1) * size] = newCellValue(neighbourCounter,
                                                                    field[(t + 1) % 2][i + (width - 1) * size]);
            }
            MPI_Send(field[t % 2], (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD);
            MPI_Send(field[t % 2] + (size * (width - 1)), (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD);

            for (size_t i = 1; i < width - 1; i++) {
                for (size_t j = 0; j < size; j++) {

                    neighbourCounter = field[t % 2][(i - 1) * size + j] + field[t % 2][(i + 1) * size + j];
                    if (j > 0) {
                        neighbourCounter += field[t % 2][(i - 1) * size + j - 1]
                                           + field[t % 2][i * size + j - 1]
                                           + field[t % 2][(i + 1) * size + j - 1];
                    }
                    else {
                        neighbourCounter += field[t % 2][(i - 1) * size + size - 1]
                                           + field[t % 2][i * size + size - 1]
                                           + field[t % 2][(i + 1) * size + size - 1];
                    }
                    if (j < size - 1) {
                        neighbourCounter += field[t % 2][(i - 1) * size + j + 1]
                                           + field[t % 2][i * size + j + 1]
                                           + field[t % 2][(i + 1) * size + j + 1];
                    }
                    else {
                        neighbourCounter += field[t % 2][(i - 1) * size]
                                           + field[t % 2][i * size]
                                           + field[t % 2][(i + 1) * size];
                    }

                    field[t % 2][i * size + j] = newCellValue(neighbourCounter, field[(t + 1) % 2][i * size + j]);
                }
            }

            MPI_Status status;
            MPI_Recv(l_neigh, (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &status);
            MPI_Recv(r_neigh, (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &status);

        }
    }

    void finish() {
        MPI_Finalize();
        free(field);
        free(l_neigh);
        free(r_neigh);
        free(fullField);
        free(sizes);
        free(starts);
    }
};

void game(size_t size, size_t turns, int argc, char **argv) {

    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

    Worker worker(argc, argv, size);
    int threads = worker.number_of_workers;
    cout << "Worker created\n";
    worker.play(size, turns);
    worker.finish();

    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "time with " << threads << "threads = " << elapsed_seconds.count() << std::endl;
}


#endif //GAMEOFLIFEMPI_FASTPLAYERMPI_H
