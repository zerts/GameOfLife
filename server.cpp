//
// Created by zerts on 04.01.17.
//

#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void writeSmth(int sockDescr, const void *buffer, int size) {
    int written = 0;
    do {
        written += (int) write(sockDescr, buffer + written * sizeof(int), (size_t) (size - written) * sizeof(int));
        //cout << "written " <<  written << endl;
    } while (written < size * sizeof(int));
}

void readSmth(int sockDescr, void *buffer, int size) {
    int alreadyRead = 0;
    do {
        alreadyRead += read(sockDescr, buffer + alreadyRead * sizeof(int), (size_t) (size - alreadyRead) * sizeof(int));
    } while (alreadyRead < size * sizeof(int));
}

const int PORT = 50100;

int main(int argc, char **argv) {

    int size, turns;
    int *fullField;
    int *workerSockets;
    int *justBuf;

    size = 2 * 2 * 2 * 2 * 2 * 2 * 3 * 3 * 5 * 5;
    turns = 50;

    fullField = (int *) malloc(size * size * sizeof(int));
    for (int i = 0; i < size * size; i++) {
        fullField[i] = (rand() > 0.5 ? 1 : 0);
    }
    fullField[0] = 1;
    fullField[1] = 1;
    fullField[size + 1] = 1;
    fullField[size + 2] = 1;
    fullField[2 * size] = 1;

    int sockDescr = socket(AF_INET, SOCK_STREAM, 0);
    if (sockDescr < 0)
    {
        perror("socket failed");
        return 0;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(sockDescr, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("first bind failed");
        return 0;
    }
    listen(sockDescr, 100000);

    std::string threads[] = {"2","4","6","8","10","12","16","18","20","24","30","32","36","40","48","60","72","80","90","96","102","144","160"};

    for(int t = 0; t < 28; t++) {
        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();




        int so_reuseaddr = 1;
        setsockopt(sockDescr, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int));

        int width = ((size + ((size % stoi(threads[t])) != 0)) / stoi(threads[t]));
        std::vector<int> clientSock((unsigned long) stoi(threads[t]));
        int curr_port = PORT;// + stoi(threads[t]) * 100;

        for (int i = 0; i < stoi(threads[t]); i++) {
            if ((curr_port + 1 + i) % 2 == 0) {

                const std::string exe = "./prog " + std::to_string((long long) (curr_port + 1 + i)) + " " + threads[t];
                //cout << exe <<  std::endl;

                int pid = fork();

                if (pid != 0) {
                    system(exe.c_str());
                    return 0;
                }

                clientSock[i] = accept(sockDescr, NULL, NULL);
                if (clientSock[i] < 0) {
                    perror("accept failed");
                    return 0;
                }
                //cout << "accepted\n";


                writeSmth(clientSock[i], fullField + (i * width * size), size * width);

                if (i == 0) {
                    writeSmth(clientSock[i], fullField + size * (size - 1), size);
                } else {
                    writeSmth(clientSock[i], fullField + i * width * size - size, size);
                }

                if (i == stoi(threads[t]) - 1) {
                    writeSmth(clientSock[i], fullField, size);
                } else {
                    writeSmth(clientSock[i], fullField + (i + 1) * width * size, size);
                }
            }
        }
        
        sleep(1);

        for (int i = 0; i < stoi(threads[t]); i++) {
            if ((curr_port + 1 + i) % 2 == 1) {

                const std::string exe = "./prog " + std::to_string((long long) (curr_port + 1 + i)) + " " + threads[t];
                //cout << exe <<  std::endl;

                int pid = fork();

                if (pid != 0) {
                    system(exe.c_str());
                    return 0;
                }

                clientSock[i] = accept(sockDescr, NULL, NULL);
                if (clientSock[i] < 0) {
                    perror("accept failed");
                    return 0;
                }
                //cout << "accepted\n";


                writeSmth(clientSock[i], fullField + (i * width * size), size * width);

                if (i == 0) {
                    writeSmth(clientSock[i], fullField + size * (size - 1), size);
                } else {
                    writeSmth(clientSock[i], fullField + i * width * size - size, size);
                }

                if (i == stoi(threads[t]) - 1) {
                    writeSmth(clientSock[i], fullField, size);
                } else {
                    writeSmth(clientSock[i], fullField + (i + 1) * width * size, size);
                }
            }
        }


        for (int i = 0; i < stoi(threads[t]); i++) {
            readSmth(clientSock[i], justBuf, sizeof(int));
        }

        //close(sockDescr);

        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        std::cout << "time with " << threads[t] << "threads = " << elapsed_seconds.count() << std::endl;
    }
    return 0;
}

