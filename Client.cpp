#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <string>
#pragma warning(disable: 4996)
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

SOCKET Connection;//для хранения сокета, представляющего соединение с сервером.

void handleError(bool err, const char* msg)
{
	if (!err)
		return;
	std::cout << msg << WSAGetLastError();
	WSACleanup();
	exit(EXIT_FAILURE);
}
void ClientHandler() {// для обработки входящих сообщений от сервера. Она бесконечно ожидает и принимает сообщения от сервера, выводит их в консоль и освобождает выделенную память.
	int msg_size;
	while (true) {
		int res = recv(Connection, (char*)&msg_size, sizeof(int), NULL);
		handleError(res == SOCKET_ERROR, "recv() error\n");
		char* msg = new char[msg_size + 1];
		msg[msg_size] = '\0';
		res = recv(Connection, msg, msg_size, NULL);
		handleError(res == SOCKET_ERROR, "recv() error\n");
		std::cout << msg << std::endl;
		delete[] msg;
	}
}

std::string GetCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&time);

	char timestamp[20];
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

	return timestamp;
}


int main(int argc, char* argv[]) {
	//чиатем конфиг файл
	// Открываем JSON-файл
	std::ifstream config_file("config.json");

	// Считываем JSON-данные из файла
	nlohmann::json config_data;
	config_file >> config_data;

	// Считываем имя пользователя
	std::string username = config_data["username"];

	std::ofstream logFile("log.txt"); //лог файл
	std::string timestamp; //для временных меток

	//WSAStartup
	WSAData wsaData;
	WORD DLLVersion = MAKEWORD(2, 1);
	int res = WSAStartup(DLLVersion, &wsaData);
	handleError(res != 0, "Error: ");
	if (WSAStartup(DLLVersion, &wsaData) != 0) {
		std::cout << "Error" << std::endl;
		exit(1);
	}

	SOCKADDR_IN addr;
	int sizeofaddr = sizeof(addr);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(1111);
	addr.sin_family = AF_INET;

	Connection = socket(AF_INET, SOCK_STREAM, NULL); //для взаимодействия с сервером.
	res = connect(Connection, (SOCKADDR*)&addr, sizeof(addr));
	handleError(res == SOCKET_ERROR, "connect fail");
	int temp_size = username.length();
	timestamp = GetCurrentTimestamp();
	logFile << "[" << timestamp << "] Пользователь отправляет свое имя серверу" << std::endl;
	res = send(Connection, (char*)&(temp_size), sizeof(int), NULL);//посылаем размер строки серверу, чтобы он знал
	handleError(res == SOCKET_ERROR, "send() failed\n");
	res = send(Connection, username.c_str(), temp_size, NULL); //отправляем самву строку
	handleError(res == SOCKET_ERROR, "send() failed\n");

	char mes[18];
	res = recv(Connection, mes, 18, NULL);
	if (res == 0 || res ==SOCKET_ERROR) {//сервер разорвал с нами соединение, так как имени нет в базе
		std::cout << "You couldn't join chat((\n";
		timestamp = GetCurrentTimestamp();
		logFile << "[" << timestamp << "] Имени пользователя нет в базе\n" << std::endl;
		WSACleanup();
		system("pause");
		return 0;
	}
	std::cout << mes;
	std::string temp;
	std::cin >> temp;
	temp_size = temp.size();
	timestamp = GetCurrentTimestamp();
	logFile << "[" << timestamp << "] Пользователь отправляет пароль" << std::endl;
	res = send(Connection, (char*)&temp_size, sizeof(int), NULL);//посылаем размер строки серверу, чтобы он знал
	handleError(res == SOCKET_ERROR, "send() failed\n");
	res = send(Connection, temp.c_str(), temp_size, NULL); //отправляем самву строку
	handleError(res == SOCKET_ERROR, "send() failed\n");

	//проверяем присоединились ли к чату
	char msgname[34];
	msgname[33] = '\0';
	res = recv(Connection, msgname, 34, NULL);
	if (res == 0 || res == SOCKET_ERROR) {
		std::cout << "You couldn't join chat((\n";
		timestamp = GetCurrentTimestamp();
		logFile << "[" << timestamp << "] Неправильный пароль\n" << std::endl;
		WSACleanup();
		system("pause");
		return 0;
	}

	std::cout << msgname;
	timestamp = GetCurrentTimestamp();
	logFile << "[" << timestamp << "] Пользователь присоединился к чату\n";


	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, NULL, NULL, NULL);// будет обрабатывать входящие сообщения
	std::string msg1;
	while (true) {
		std::getline(std::cin, msg1);
		//валидация сообщения
		int msg_size = msg1.size();
		bool er = false;
		for (char x : msg1) {
			if (x < 32) {
				std::cout << "Your message won't be sent, enter valid characters\n";
				er = true;
				break;
			}
		}
		if (er) { 
			timestamp = GetCurrentTimestamp();
			logFile << "[" << timestamp << "] Пользователь отправил невалидное сообщение(с символами, чей код <= 32)" << std::endl;
			continue; }
		if (msg1 == "leave chat") { //покидаем чат
			res = closesocket(Connection);
			handleError(res == SOCKET_ERROR, "closesocket() failed\n");
			std::cout << "You successfully left chat\n";
			timestamp = GetCurrentTimestamp();
			logFile << "[" << timestamp << "] Пользователь покинул чат" << std::endl;
			WSACleanup();
			system("pause");
			return 0;
		}
		res = send(Connection, (char*)&msg_size, sizeof(int), NULL);//посылаем размер строки серверу, чтобы он знал
		handleError(res == SOCKET_ERROR, "send() failed\n");
		res = send(Connection, msg1.c_str(), msg_size, NULL); //отправляем самву строку
		handleError(res == SOCKET_ERROR, "send() failed\n");
		timestamp = GetCurrentTimestamp();
		logFile << "[" << timestamp << "] Пользователь отправил сообщение в чат" << std::endl;
		Sleep(10);
	}

	WSACleanup();
	system("pause");
	return 0;
}
