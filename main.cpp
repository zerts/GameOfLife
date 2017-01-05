#include <iostream>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

using std::make_pair;
using std::thread;
using std::ref;

std::mutex mtx;
std::condition_variable cv;

int SERVER_PORT = 50100;
const int TURNS = 50;
const int SIZE = 2 * 2 * 2 * 2 * 2 * 2 * 3 * 3 * 5 * 5;

std::vector< std::pair<size_t, size_t> > getNeighbours(size_t i, size_t j, size_t size) {
    std::vector< std::pair<size_t, size_t> > result;
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

void writeSmth(int sockDescr, const void *buffer, size_t size) {
    int written = 0;
    do {
        written += (int) write(sockDescr, buffer + written, size * sizeof(int) - written);
    } while (written < size * sizeof(int));
}

void readSmth(int sockDescr, void *buffer, int size) {
    int alreadyRead = 0;
    do {
        alreadyRead += read(sockDescr, buffer + alreadyRead, size * sizeof(int) - alreadyRead);
        //cout << "already read " << alreadyRead << endl;
    } while (alreadyRead < size * sizeof(int));
}

void listener(int sock, int *buf, int size, int turns, int &curr_turn) {
    while(curr_turn < turns) {
        readSmth(sock, buf, size);
        curr_turn++;
        cv.notify_all();
    }
}

class Worker{
    int size, width, turns;
    int curr_port, l_number, r_number, readSock, serverSock;
    int *field[2], *l_neigh, *r_neigh;


public:
    int number_of_workers;
    int sock;


    Worker(int currSize, int currWidth, int currTurns, int currSock, int currNumber, int currThreads) {
        size = currSize;
        width = currWidth;
        turns = currTurns;
        number_of_workers = currThreads;
        curr_port = currNumber;
        sock = currSock;

        field[0] = (int *) malloc(size * width * sizeof(int));
        field[1] = (int *) malloc(size * width * sizeof(int));
        l_neigh = (int *) malloc(size * sizeof(int));
        r_neigh = (int *) malloc(size * sizeof(int));

    }

    void init() {
        readSmth(sock, field[0], size * width);

        readSmth(sock, l_neigh, size);
        readSmth(sock, r_neigh, size);
    }

    int next_port(int port) {
        return ((port - SERVER_PORT - 1) + 1) % number_of_workers + (SERVER_PORT + 1);
    }

    int prev_port(int port) {
        return ((port - SERVER_PORT - 1) + number_of_workers - 1) % number_of_workers + (SERVER_PORT + 1);
    }


    void connecting() {
        if (curr_port % 2 == 0) {
            int curr_sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons((uint16_t) curr_port);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            int so_reuseaddr = 1;
            setsockopt(curr_sock, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int));

            //cout << "curr_port = " << curr_port << endl;
            if (bind(curr_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                perror("bind in client failed");
                exit(2);
            }
            listen(curr_sock, 10000);

            //cout << "curr_port = " << curr_port << endl;

            int neigh1, neigh2, is_swap;
            neigh1 = accept(curr_sock, NULL, NULL);
            neigh2 = accept(curr_sock, NULL, NULL);
            readSmth(neigh1, &is_swap, 1);
            readSmth(neigh2, &is_swap, 1);
            if (is_swap) {
                l_number = neigh1;
                r_number = neigh2;
            }
            else {
                l_number = neigh2;
                r_number = neigh1;
            }
        }
        else {
            l_number = socket(AF_INET, SOCK_STREAM, 0);
            r_number = socket(AF_INET, SOCK_STREAM, 0);

            struct sockaddr_in neigh_addr;

            neigh_addr.sin_family = AF_INET;
            neigh_addr.sin_port = htons((uint16_t) prev_port(curr_port));
            neigh_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            //cout << "number of threads = " << number_of_workers << endl;
            //cout << "next_port = " << next_port(curr_port) << endl;
            //cout << "prev_port = " << prev_port(curr_port) << endl;
            //cout << "curr_port = " << curr_port << endl;

            if (connect(l_number, (struct sockaddr *)&neigh_addr, sizeof(neigh_addr)) < 0)
            {
                perror("connect with left failed");
                exit(2);
            }

            neigh_addr.sin_port = htons((uint16_t) next_port(curr_port));
            if (connect(r_number, (struct sockaddr *)&neigh_addr, sizeof(neigh_addr)) < 0)
            {
                perror("connect with right failed");
                exit(2);
            }
            int ONE = 1, ZERO = 0;
            writeSmth(l_number, &ONE, 1);
            writeSmth(r_number, &ZERO, 1);
        }
    }

    void play() {
        int curr_turn_l = 0, curr_turn_r = 0;
        int *l_buf = (int *) malloc(size * sizeof(int));
        int *r_buf = (int *) malloc(size * sizeof(int));
        thread left_listener = thread(listener, l_number, l_buf, size, turns, ref(curr_turn_l));
        thread right_listener = thread(listener, r_number, r_buf, size, turns, ref(curr_turn_r));

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

                field[(t + 1) % 2][i] = newCellValue(neighbourCounter, field[t % 2][i]);

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

                field[(t + 1) % 2][i + (width - 1) * size] = newCellValue(neighbourCounter,
                                                                          field[t % 2][i + (width - 1) * size]);
            }

            writeSmth(l_number, field[t % 2], (size_t) size);
            writeSmth(r_number, field[t % 2] + (size * (width - 1)), (size_t) size);

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

                    field[(t + 1) % 2][i * size + j] = newCellValue(neighbourCounter, field[t % 2][i * size + j]);
                }
            }
            while(curr_turn_l < t) {
                std::unique_lock<std::mutex> lck(mtx);
                cv.wait(lck);
            }
            memcpy(l_neigh, l_buf, size * sizeof(int));

            while(curr_turn_r < t) {
                std::unique_lock<std::mutex> lck(mtx);
                cv.wait(lck);
            }
            memcpy(r_neigh, r_buf, size * sizeof(int));
        }
        left_listener.join();
        right_listener.join();
        close(l_number);
        close(r_number);
    }
};

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int threads = atoi(argv[2]);

    struct sockaddr_in addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket failed");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect failed");
        exit(2);
    }

    int width = ((SIZE + ((SIZE % threads) != 0)) / threads);

    Worker worker(SIZE, width, TURNS, sock, port, threads);
    worker.init();
    worker.connecting();
    //std::cout << "connected\n";
    worker.play();
    //cout << "played\n";

    writeSmth(sock, &port, 1);

    close(sock);
    return 0;
}
