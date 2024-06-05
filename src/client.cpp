#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <thread>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

const char* SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8080;
char gameBoard[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
sf::RenderWindow window(sf::VideoMode(600, 600), "Tic Tac Toe");
sf::Font font;
bool myTurn = false; // Flag to indicate if it's the player's turn
std::mutex gameBoardMutex;
std::condition_variable cv;
bool updated = false; // Flag to indicate if the board was updated
char playerSymbol = ' '; // The symbol (X or O) representing this client
bool chatMode = false; // Flag to indicate if the client is in chat mode

std::unordered_map<int, std::pair<int, int>> positionMap = {
    {1, {0, 0}}, {2, {0, 1}}, {3, {0, 2}},
    {4, {1, 0}}, {5, {1, 1}}, {6, {1, 2}},
    {7, {2, 0}}, {8, {2, 1}}, {9, {2, 2}}
};

void drawBoard() {
    std::lock_guard<std::mutex> lock(gameBoardMutex);
    window.clear(sf::Color::White);

    sf::RectangleShape line(sf::Vector2f(600, 5));
    line.setFillColor(sf::Color::Black);

    for (int i = 1; i < 3; ++i) {
        line.setPosition(0, i * 200);
        window.draw(line);
    }

    line.setSize(sf::Vector2f(5, 600));
    for (int i = 1; i < 3; ++i) {
        line.setPosition(i * 200, 0);
        window.draw(line);
    }

    sf::Text text("", font, 100);
    text.setFillColor(sf::Color::Black);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            text.setString(gameBoard[i][j]);
            text.setPosition(j * 200 + 75, i * 200 + 50);
            window.draw(text);
        }
    }

    window.display();
}

void receiveUpdates(sf::TcpSocket& socket) {
    char buffer[1024];
    while (true) {
        std::size_t received;
        if (socket.receive(buffer, sizeof(buffer), received) != sf::Socket::Done) {
            std::cerr << "Failed to receive data from server." << std::endl;
            break;
        }
        buffer[received] = '\0';
        std::string message(buffer);

        {
            std::lock_guard<std::mutex> lock(gameBoardMutex);
            if (message.substr(0, 5) == "MOVE:") {
                int row = message[5] - '0';
                int col = message[7] - '0';
                char player = message[9];
                gameBoard[row][col] = player;
                std::cout << "Received move: " << message << std::endl;
                myTurn = (player != playerSymbol);
                updated = true;
            } else if (message.substr(0, 5) == "CHAT:") {
                std::cout << "Chat: " << message.substr(5) << std::endl;
            } else if (message.substr(0, 7) == "WINNER:") {
                std::cout << "Winner: " << message.substr(7) << std::endl;
                memset(gameBoard, ' ', sizeof(gameBoard));
                updated = true;
            } else if (message == "DRAW") {
                std::cout << "The game is a draw." << std::endl;
                memset(gameBoard, ' ', sizeof(gameBoard));
                updated = true;
            } else if (message.substr(0, 6) == "STATE:") {
                int index = 6;
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        gameBoard[i][j] = message[index++];
                    }
                }
                std::cout << "Received game state update." << std::endl;
                updated = true;
                myTurn = (playerSymbol == 'X');
            }
        }
        cv.notify_one();
    }
}

void request_move_or_chat(sf::TcpSocket& socket) {
    while (true) {
        if (!chatMode && myTurn) {
            std::cout << "Enter the number corresponding to the block (1-9), type '0' to chat, or 'reset' to reset the game: ";
            std::string input;
            std::cin >> input;

            if (input == "reset") {
                if (socket.send("RESET", strlen("RESET")) != sf::Socket::Done) {
                    std::cerr << "Failed to send reset command to server." << std::endl;
                } else {
                    std::cout << "Sent reset command to server." << std::endl;
                }
            } else if (input == "0") {
                chatMode = true;
                std::cout << "Chat mode. Type your message and press Enter, or type 'q' to quit chat mode.\n";
            } else {
                int block = std::stoi(input);
                if (positionMap.find(block) != positionMap.end()) {
                    int row = positionMap[block].first;
                    int col = positionMap[block].second;
                    {
                        std::lock_guard<std::mutex> lock(gameBoardMutex);
                        if (gameBoard[row][col] == ' ') {
                            char move[20];
                            snprintf(move, sizeof(move), "MOVE:%d,%d,%c", row, col, playerSymbol);
                            if (socket.send(move, strlen(move)) != sf::Socket::Done) {
                                std::cerr << "Failed to send move to server." << std::endl;
                            } else {
                                std::cout << "Sent move: " << move << std::endl;
                                myTurn = false;
                            }
                        } else {
                            std::cout << "Invalid move, try again." << std::endl;
                        }
                    }
                } else {
                    std::cout << "Invalid input, try again." << std::endl;
                }
            }
        }

        if (chatMode) {
            std::string chatMessage;
            std::getline(std::cin, chatMessage);

            if (chatMessage == "q") {
                chatMode = false;
                std::cout << "Exited chat mode. You can now enter moves.\n";
            } else {
                std::string message = "CHAT:" + chatMessage;
                if (socket.send(message.c_str(), message.size()) != sf::Socket::Done) {
                    std::cerr << "Failed to send chat message to server." << std::endl;
                } else {
                    std::cout << "Sent chat message: " << chatMessage << std::endl;
                }
            }
        }

        {
            std::unique_lock<std::mutex> lock(gameBoardMutex);
            if (cv.wait_for(lock, std::chrono::milliseconds(100), [] { return updated; })) {
                updated = false;
                lock.unlock();
                window.setActive(true);
                drawBoard();
                window.setActive(false);
            }
        }
    }
}

int main() {
    std::cout << "Starting client..." << std::endl;

    if (!font.loadFromFile("/home/gabi/Desktop/TicTacToe/src/arial.ttf")) {
        std::cerr << "Failed to load font" << std::endl;
        return -1;
    }

    std::cout << "Font loaded successfully." << std::endl;

    sf::TcpSocket socket;
    sf::Socket::Status status = socket.connect(SERVER_IP, SERVER_PORT);
    if (status != sf::Socket::Done) {
        std::cerr << "Failed to connect to server" << std::endl;
        return -1;
    }

    std::cout << "Connected to server." << std::endl;

    char buffer[2];
    std::size_t received;
    if (socket.receive(buffer, sizeof(buffer), received) != sf::Socket::Done) {
        std::cerr << "Failed to receive player symbol from server." << std::endl;
        return -1;
    }
    playerSymbol = buffer[0];
    myTurn = (playerSymbol == 'X');

    std::cout << "Player symbol: " << playerSymbol << std::endl;

    std::thread receiveThread(receiveUpdates, std::ref(socket));

    drawBoard();

    request_move_or_chat(socket);

    receiveThread.join();

    return 0;
}

