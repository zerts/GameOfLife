#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <mpi.h>
#include <stdlib.h>
#include <malloc.h>

using namespace std;

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
            fullField[i] = 0;//(rand() > 0.5 ? 1 : 0);
        }
        fullField[0] = 1;
        fullField[1] = 1;
        fullField[size + 1] = 1;
        fullField[size + 2] = 1;
        fullField[2 * size] = 1;

        /*for (size_t i = 0; i < size; i++) {
            for (size_t j = 0; j < size; j++) {
                cout << fullField[i * size + j] << ' ';
            }
            cout << endl;
        }
        cout << endl << endl;*/

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

        if (curr_number == 0) {
            readAllField(size);
        }

        MPI_Scatterv(fullField, sizes, starts, MPI_INT, field[0], (int) (size * width), MPI_INT, 0, MPI_COMM_WORLD);


        MPI_Status status;
        MPI_Request request;
        MPI_Isend(field[0], (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &request);
        MPI_Isend(field[0] + (size * (width - 1)), (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &request);

        MPI_Recv(r_neigh, (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(l_neigh, (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &status);

        /*for (size_t j = 0; j < size; j++) {
            cout << l_neigh[j] << ' ';
        }
        cout << endl;*/
    }

    void play(size_t size, size_t turns) {
        int neighbourCounter;
        for (size_t t = 0; t < turns; t++) {

            for (size_t i = 0; i < size; i++) {



                neighbourCounter = l_neigh[i] + field[t % 2][size + i];

                /*if (i == 0 && curr_number == 0) {
                    for (size_t j = 0; j < size; j++) {
                        cout << l_neigh[j] << ' ';
                    }
                    cout << endl;
                    cout << neighbourCounter << endl;
                }*/

                if (i > 0) {
                    neighbourCounter += l_neigh[i - 1]
                                        + field[t % 2][size + i - 1]
                                        + field[t % 2][i - 1];
                } else {
                    neighbourCounter += l_neigh[size - 1]
                                        + field[t % 2][2 * size - 1]
                                        + field[t % 2][size - 1];
                }
                if (i < size - 1) {
                    neighbourCounter += l_neigh[i + 1]
                                        + field[t % 2][size + i + 1]
                                        + field[t % 2][i + 1];
                } else {
                    neighbourCounter += l_neigh[0]
                                        + field[t % 2][size]
                                        + field[t % 2][0];
                }

                field[(t + 1) % 2][i] = newCellValue(neighbourCounter, field[t % 2][i]);

                if (i == 0 && curr_number == 0) {
                    cout << neighbourCounter << endl;
                }


                neighbourCounter = r_neigh[i] + field[t % 2][(width - 2) * size + i];
                if (i > 0) {
                    neighbourCounter += r_neigh[i - 1]
                                        + field[t % 2][(width - 2) * size + i - 1]
                                        + field[t % 2][(width - 1) * size + i - 1];
                } else {
                    neighbourCounter += r_neigh[size - 1]
                                        + field[t % 2][(width - 2) * size + size - 1]
                                        + field[t % 2][(width - 1) * size + size - 1];
                }
                if (i < size - 1) {
                    neighbourCounter += r_neigh[i + 1]
                                        + field[t % 2][(width - 2) * size + i + 1]
                                        + field[t % 2][(width - 1) * size + i + 1];
                } else {
                    neighbourCounter += r_neigh[0]
                                        + field[t % 2][(width - 2) * size]
                                        + field[t % 2][(width - 1) * size];
                }

                field[(t + 1) % 2][i + (width - 1) * size] = newCellValue(neighbourCounter,
                                                                    field[t % 2][i + (width - 1) * size]);
            }
            MPI_Request request1, request2;
            MPI_Isend(field[t % 2], (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &request1);
            MPI_Isend(field[t % 2] + (size * (width - 1)), (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &request2);

            for (size_t i = 1; i < width - 1; i++) {
                for (size_t j = 0; j < size; j++) {

                    neighbourCounter = field[t % 2][(i - 1) * size + j] + field[t % 2][(i + 1) * size + j];
                    if (j > 0) {
                        neighbourCounter += field[t % 2][(i - 1) * size + j - 1]
                                            + field[t % 2][i * size + j - 1]
                                            + field[t % 2][(i + 1) * size + j - 1];
                    } else {
                        neighbourCounter += field[t % 2][(i - 1) * size + size - 1]
                                            + field[t % 2][i * size + size - 1]
                                            + field[t % 2][(i + 1) * size + size - 1];
                    }
                    if (j < size - 1) {
                        neighbourCounter += field[t % 2][(i - 1) * size + j + 1]
                                            + field[t % 2][i * size + j + 1]
                                            + field[t % 2][(i + 1) * size + j + 1];
                    } else {
                        neighbourCounter += field[t % 2][(i - 1) * size]
                                            + field[t % 2][i * size]
                                            + field[t % 2][(i + 1) * size];
                    }

                    field[(t + 1) % 2][i * size + j] = newCellValue(neighbourCounter, field[t % 2][i * size + j]);
                }
            }

            MPI_Status status;
            MPI_Recv(l_neigh, (int) size, MPI_INT, l_number, 0, MPI_COMM_WORLD, &status);
            MPI_Recv(r_neigh, (int) size, MPI_INT, r_number, 0, MPI_COMM_WORLD, &status);
        }

    }

    void finish(size_t size, size_t turns) {
        MPI_Gatherv(field[turns % 2], size * width, MPI_INT, fullField, sizes, starts, MPI_INT, 0, MPI_COMM_WORLD);

        /*if (curr_number == 0) {
            for (size_t i = 0; i < size; i++) {
                for (size_t j = 0; j < size; j++) {
                    cout << fullField[i * size + j] << ' ';
                }
                cout << endl;
            }
        }*/

        MPI_Finalize();
    }
};

void game(Worker worker, size_t size, size_t turns, int argc, char **argv) {

    int threads = worker.number_of_workers;
    worker.play(size, turns);
    worker.finish(size, turns);
}

int main(int argc, char **argv) {
    int size, turns;
    size = 2 * 2 * 2 * 2 * 2 * 3 * 3 * 5;
    turns = 5;

    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    Worker worker(argc, argv, size);
    game(worker, (size_t) size, (size_t) turns, argc, argv);
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "time = " << elapsed_seconds.count() << std::endl;

    return 0;
}


/*
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <stdlib.h>
#include <malloc.h>
#include <string>

int main() {
        std::string threads[] = {"1","2","3","4","5","6","8","9","10","12","15","16","18","20","24","30","32","36","40","45","48","60","72","80","90","96","102","144","160"};
        for(int i = 0; i < 28; i++) {
                std::chrono::time_point<std::chrono::system_clock> start, end;
                start = std::chrono::system_clock::now();

                const std::string exe = "mpirun -np " + threads[i] + " prog";
                //std::cout << exe <<  std::endl;
                system(exe.c_str());

                end = std::chrono::system_clock::now();
                std::chrono::duration<double> elapsed_seconds = end - start;
                std::cout << "TIME with " + threads[i] + " threads = " << elapsed_seconds.count() << std::endl;
        }
        return 0;
}

 */