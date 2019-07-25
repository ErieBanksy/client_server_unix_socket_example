#include <iostream>
#include <fstream>
#include <string>
#include <signal.h>

#include <thread>
#include <pthread.h>

#include <unistd.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

using std::cout;
using std::endl;
using std::string;
using std::thread;

typedef int socket_t;

void serverOperation();
void clientOperation();
void sigHandler(int sig);
bool initUnixSocketPath(string path);
bool initUnixSocket();
void waitEndingProgramm();
volatile sig_atomic_t force_exit = false;

int maxSizeDataFromUS = 1024;

//Дескрипторы слушаещего и принимающего(отправляющего) сокетов
socket_t listenSocketd;
socket_t duplexSocketd;

struct sockaddr_un domainSockAddr; //Адрес доменного сокета UNIX
char unixSocketPath[50];


int main() {
    signal(SIGINT, sigHandler);

    string unixSocketPathTest = "/home/pi/test.socket";
    if (initUnixSocketPath(unixSocketPathTest) == false) {
        cout << "US: Unix socket path not initialized" << endl;
        return -1;
    }
    if (initUnixSocket() == false) {
        cout  << "US: Unix socket not initialized" << endl;
        return -1;
    }

    thread(clientOperation).detach();
    thread(serverOperation).detach();

    sleep(2);
	
	waitEndingProgramm();

    return 0;
}


/**
* \brief Функция работы сервера.
*
* Пока не будет совершен выход из программы, сервер будет работать в потоке, 
* получая и отправляя сообщения.
*
*/
void serverOperation() {
    char readData[maxSizeDataFromUS] = { 0 };
    int countByteFromSock = 0;

    while (!force_exit) {
        // Создается новый сокет, который принимает и отправляет сообщения
        if ((duplexSocketd = accept(listenSocketd, NULL, NULL)) == -1) {
            cout << "US: Accept error" << endl;
            continue;
        }

        while ((countByteFromSock = read(duplexSocketd, readData, maxSizeDataFromUS)) > 0) {
            string recvJson(readData);
            if (recvJson.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            cout << "\x1b[34mServer\x1b[0m recv: " << recvJson << endl;

            countByteFromSock = 0;
            for (int i = 0; i < maxSizeDataFromUS; i++) {
                readData[i] = 0;
            }
			
			string sendJson = "Hello client! Nice to meet you!";
			if (write(duplexSocketd, sendJson.c_str(), sendJson.size()) < 0) {
				cout << "US: ERROR writing to socket in " << __FUNCTION__ << endl;
				pthread_exit(NULL);
			}
			
			cout << "\x1b[34mServer\x1b[0m send: " << sendJson << endl;
        } 

        std::this_thread::sleep_for(std::chrono::milliseconds(850));

    } 
    pthread_exit(NULL);
}


/**
* \brief Функция работы клиента.
*
* Пока не будет совершен выход из программы, клиент будет отправлять приветственное 
* сообщение серверу и принимать ответ на него.
*
*/
void clientOperation() {
    socket_t sentSocketd;
    struct sockaddr_un domainSockAddr;
    char readData[maxSizeDataFromUS] = { 0 };
    char unixSocketPath[50] = "/home/pi/test.socket";

    if ((sentSocketd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        cout << "ERROR: Unix socket error..." << endl;
        pthread_exit(NULL);
    }
    
    domainSockAddr.sun_family = AF_UNIX;//указываем семейство адресов
    if (*unixSocketPath == '\0') {
        cout << "ERROR: Unix socket path is empty" << endl;
        pthread_exit(NULL);
    }
    else {
        strcpy(domainSockAddr.sun_path, unixSocketPath);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	
    if (connect(sentSocketd, (struct sockaddr*)&domainSockAddr, sizeof(domainSockAddr)) < 0) {
        cout << "ERROR: connect error..." << endl;
        pthread_exit(NULL);
    }

    while (!force_exit) {
		string sendJson = "Hello server! I'm client.";
        if (write(sentSocketd, sendJson.c_str(), sendJson.size()) < 0) {
            cout << "US: ERROR writing to socket" << endl;
            pthread_exit(NULL);
        }
		cout << "\x1b[33mClient\x1b[0m send: " << sendJson << endl;;
		
        if (read(sentSocketd, readData, maxSizeDataFromUS) > 0) {
			string resvJson(readData);

            if (resvJson.empty()) {
                cout << "Error: data.empty()" << endl;
                pthread_exit(NULL);
            }

            cout << "\x1b[33mClient\x1b[0m recv: " << resvJson << endl;
			
			for (int i = 0; i < maxSizeDataFromUS; i++) {
				readData[i] = 0;
			}
		}

		sleep(10);
    }

    close(sentSocketd);
    pthread_exit(NULL);
}


/**
* \brief Функция инициализации unix сокета.
*
* Пока не будет совершен выход из программы, клиент будет отправлять приветственное 
* сообщение серверу и принимать ответ на него.
*
*/
bool initUnixSocket() {
    if ((listenSocketd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        cout << "ERROR: Unix socket error..." << endl;
        return false;
    }

    domainSockAddr.sun_family = AF_UNIX;//указываем семейство адресов
    if (*unixSocketPath == '\0') {
        cout << "ERROR: Unix socket path is empty" << endl;
        return false;
    }
    else {
        for (size_t i = 0; i < sizeof(unixSocketPath); i++) {
            domainSockAddr.sun_path[i] = unixSocketPath[i];
        }
        unlink(unixSocketPath);//привязка сокета к имени файла 
    }

    // Создает файл test.socket 
    if (bind(listenSocketd, (struct sockaddr*)&domainSockAddr, sizeof(domainSockAddr)) == -1) {
        cout << "ERROR: Unix socket bind error" << endl;
        return false;
    }

    // Уставливает режим доступа к файлу, 0777 - полный доступ
    chmod(unixSocketPath, 0777);

    // Создаётся очередь запросов на соединение 
    // Сокет переводится в режим ожидания запросов со стороны клиентов
    if (listen(listenSocketd, 1) == -1) {
        cout << "ERROR: Unix socket listen error" << endl;
        return false;
    }

    return true;
}


/**
* \brief Функция инициализации пути unix сокета.
*/
bool initUnixSocketPath(string path) {
    if (path.size() == 0) {
        cout << "Error: unixSocketPath is empty in " << __FUNCTION__ << endl;
        return false;
    }
    for (size_t i = 0; i < path.size(); i++) {
        unixSocketPath[i] = path[i];
    }
    return true;
}


/**
* \brief Функция обработчик полученного сигнала. 
*
* Нужна для завершения работы программы.
*
* \param [in] sig - полученный сигнал.
*/
void sigHandler(int sig) {
    cout << "Break received, exiting!" << endl;
    force_exit = true;
}


/**
* \brief Функция ожидания завершения работы программы. 
*/
void waitEndingProgramm() {
    while (!force_exit) {
        sleep(10);
    }
}
