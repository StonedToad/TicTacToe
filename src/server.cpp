#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

const int PORT = 8080;
std::vector<int> clients;
std::mutex clients_mutex;

char gameBoard[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
char currentPlayer = 'X';

void broadcast_message(const std::string& message, int exclude_socket = -1) {
    std::lock_guard<std::mutex> guard(clients_mutex);
    for (int client_socket : clients) {
        if (client_socket != exclude_socket) {
            send(client_socket, message.c_str(), message.size(), 0);
        }
    }
}

bool check_winner() {
    for (int i = 0; i < 3; ++i) {
        if (gameBoard[i][0] == currentPlayer && gameBoard[i][1] == currentPlayer && gameBoard[i][2] == currentPlayer)
            return true;
        if (gameBoard[0][i] == currentPlayer && gameBoard[1][i] == currentPlayer && gameBoard[2][i] == currentPlayer)
            return true;
    }
    if (gameBoard[0][0] == currentPlayer && gameBoard[1][1] == currentPlayer && gameBoard[2][2] == currentPlayer)
        return true;
    if (gameBoard[0][2] == currentPlayer && gameBoard[1][1] == currentPlayer && gameBoard[2][0] == currentPlayer)
        return true;

    return false;
}

bool check_draw() {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (gameBoard[i][j] == ' ')
                return false;
        }
    }
    return true;
}

void reset_game() {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            gameBoard[i][j] = ' ';
        }
    }
    currentPlayer = 'X';
}

void send_game_state(int client_socket) {
    std::string game_state = "STATE:";
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            game_state += gameBoard[i][j];
        }
    }
    send(client_socket, game_state.c_str(), game_state.size(), 0);
}

void broadcast_game_state() {
    std::string game_state = "STATE:";
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            game_state += gameBoard[i][j];
        }
    }
    broadcast_message(game_state);
}

void handle_client(int client_socket) {
    {
        std::lock_guard<std::mutex> guard(clients_mutex);
        clients.push_back(client_socket);
    }

    char playerSymbol = (clients.size() == 1) ? 'X' : 'O';
    send(client_socket, &playerSymbol, 1, 0);
    send_game_state(client_socket);

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0) {
            std::lock_guard<std::mutex> guard(clients_mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
            close(client_socket);
            break;
        }
        std::string message(buffer);

        if (message.substr(0, 5) == "MOVE:") {
            int row = message[5] - '0';
            int col = message[7] - '0';
            if (gameBoard[row][col] == ' ' && (currentPlayer == message[9])) {
                gameBoard[row][col] = currentPlayer;
                std::string move_message = "MOVE:" + std::to_string(row) + "," + std::to_string(col) + "," + currentPlayer;
                broadcast_message(move_message);

                std::cout << "Processed move: " << move_message << std::endl;

                if (check_winner()) {
                    std::string win_message = "WINNER:";
                    win_message += currentPlayer;
                    broadcast_message(win_message);
                    std::cout << "Player " << currentPlayer << " wins!" << std::endl;
                    reset_game();
                    broadcast_game_state();
                } else if (check_draw()) {
                    std::string draw_message = "DRAW";
                    broadcast_message(draw_message);
                    std::cout << "Game is a draw." << std::endl;
                    reset_game();
                    broadcast_game_state();
                } else {
                    currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
                }

                broadcast_game_state();
            }
        } else if (message.substr(0, 5) == "CHAT:") {
            broadcast_message(message, client_socket);
        } else if (message == "RESET") {
            std::cout << "Resetting game..." << std::endl;
            reset_game();
            broadcast_game_state();
        }
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running and waiting for connections..." << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        std::cout << "New client connected." << std::endl;
        std::thread(handle_client, new_socket).detach();
    }

    return 0;
}

