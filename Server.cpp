#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#pragma warning(disable: 4996)
#include <nlohmann/json.hpp>

SOCKET Connections[100];//для хранения сокетов клиентов 
int Counter = 0;// количества подключенных клиентов
struct Client {
	SOCKET socket = 0;
	std::string username;
	std::string password;
	Client(std::string name, std::string pass) : username(name), password(pass) {
	}
};
int numberOfClientsInDataBase = 4; //число пользователей в базе
std::vector<Client> Clients; //для хранения данных пользователей



void handleError(bool err, const char* msg)
{
	if (!err)
		return;
	std::cout << msg << WSAGetLastError();
	WSACleanup();
	exit(EXIT_FAILURE);
}
// Функция для получения текущей временной метки в виде строки
std::string GetCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&time);

	char timestamp[20];
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

	return timestamp;
}

void ClientHandler(int index) {//обрабатывает сообщения от конкретного клиента (индекс передается как параметр). Она бесконечно ожидает сообщения от клиента, затем отправляет это сообщение всем остальным подключенным клиентам
	int msg_size;
	while (true) {
		int res = recv(Connections[index], (char*)&msg_size, sizeof(int), NULL);
		char* msg = new char[msg_size + 1];
		msg[msg_size] = '\0';
		res = recv(Connections[index], msg, msg_size, NULL);
		if (res == SOCKET_ERROR) {//клиент разорваал соединение с сервером
			std::cout << "Client left chat\n";
			std::ofstream logFile("log.txt", std::ios::app);
			std::string timestamp = GetCurrentTimestamp();
			logFile << "[" << timestamp << "] Клиент номер " << index+1   << " покинул чат" << std::endl;
			return;

		}
		for (int i = 0; i < Counter; i++) {//разослать сообщ другим участникам
			if (i == index) {
				continue;
			}

			send(Connections[i], (char*)&msg_size, sizeof(int), NULL);
			send(Connections[i], msg, msg_size, NULL);
		}
		delete[] msg;
	}
}

int main(int argc, char* argv[]) {
	//WSAStartup

	std::ofstream logFile("log.txt"); //лог файл
	std::string timestamp; //для временных меток

	std::ifstream config_file("config.json");
	// Считываем из JSON конфигурационного файла данные о пользователях(имя пароль)
	nlohmann::json config_data;
	config_file >> config_data;

	// Получаем значение имени пользователя
	for (int i = 0; i < numberOfClientsInDataBase; ++i) {
		std::string tmp_name = "username " + std::to_string(i + 1);
		std::string tmp_password = "userpassword " + std::to_string(i + 1);
		std::string username = config_data[tmp_name];
		std::string userpassword = config_data[tmp_password];
		Clients.emplace_back(username, userpassword);
	}
	WSAData wsaData;
	WORD DLLVersion = MAKEWORD(2, 1);
	if (WSAStartup(DLLVersion, &wsaData) != 0) {
		std::cout << "Error" << std::endl;
		exit(1);
	}

	SOCKADDR_IN addr;
	int sizeofaddr = sizeof(addr);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(1111);
	addr.sin_family = AF_INET;

	SOCKET sListen = socket(AF_INET, SOCK_STREAM, NULL);// для прослушивания входящих соединений
	bind(sListen, (SOCKADDR*)&addr, sizeof(addr));//Привязка сокета к адресу сервера

	listen(sListen, SOMAXCONN);//Прослушивание входящих соединений

	SOCKET newConnection;
	int not_existing_clients = 0;//число клиентов которых нет в базе
	for (int i = 0; i < 100; i++) {
		newConnection = accept(sListen, (SOCKADDR*)&addr, &sizeofaddr);//Принятие входящих соединений

		timestamp = GetCurrentTimestamp();
		logFile << "[" << timestamp << "] Клиент номер "<<i<<"пытается подключится к серверу" << std::endl;
		
		handleError(newConnection == SOCKET_ERROR, "no connection");

		int msg_size;

		int res = recv(newConnection, (char*)&msg_size, sizeof(int), NULL);
		handleError(res == SOCKET_ERROR, "recv() failed");
		char* msgname = new char[msg_size + 1];
		msgname[msg_size] = '\0';
		res = recv(newConnection, msgname, msg_size, NULL);
		handleError(res == SOCKET_ERROR, "recv() failed");
		bool found = false;//клиент есть в базе
		for (auto x : Clients) {
			if (x.username == std::string(msgname)) {
				if (x.socket != 0) {//значит клиент уже был подключен
					break;
				}
				timestamp = GetCurrentTimestamp();
				logFile << "[" << timestamp << "] " << "имя " << std::string(msgname)<< " есть в базе, запрашиваем у пользователя пароль"<< std::endl;
				
				found = true;

				res = send(newConnection, "Enter password:  ", 18, NULL);
				handleError(res == SOCKET_ERROR, "send() failed");

				recv(newConnection, (char*)&msg_size, sizeof(int), NULL);
				handleError(res == SOCKET_ERROR, "recv() failed");
				char* msgpassw = new char[msg_size + 1];
				msgpassw[msg_size] = '\0';
				recv(newConnection, msgpassw, msg_size, NULL);
				handleError(res == SOCKET_ERROR, "recv() failed");
				if (x.password == std::string(msgpassw)) {
					x.socket = newConnection;
					found = true;
					timestamp = GetCurrentTimestamp();
					logFile << "[" << timestamp << "] Клиент номер " <<i <<", с именем "  << std::string(msgname) << " подключился к чату" << std::endl;
					break;
				}
				else {
					timestamp = GetCurrentTimestamp();
					logFile << "[" << timestamp << "] "  << " пароли не совпадают" << std::endl;
					found = false;
				}
			}
		}
		delete[] msgname;
		if (!found) {
			timestamp = GetCurrentTimestamp();
			logFile << "[" << timestamp << "] " << "отключаем клиента номер " << i << " от сервера" << std::endl;
			++not_existing_clients;
			closesocket(newConnection);
			continue;
		}
		else {
			msg_size;
			char mesAboutSuccecfulConnectionToChat[] = "You're successfully joined chat\n";
			send(newConnection, mesAboutSuccecfulConnectionToChat, 33, NULL); //отправляем саму строку
			handleError(res == SOCKET_ERROR, "send() failed");
			logFile << "[" << timestamp << "] " << "клиент номер " << i << " подключился к чату" << std::endl;
		}
		std::cout << "Client Connected!\n";
		Connections[i-not_existing_clients] = newConnection;
		Counter++;
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, (LPVOID)(i), NULL, NULL);//для выполнения функции ClientHandler для каждого клиента.
	}

	WSACleanup();
	system("pause");
	return 0;
}
