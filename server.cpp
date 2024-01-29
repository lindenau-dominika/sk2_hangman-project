#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// TODO
// wysylanie bledow ze strony serwera
// testowanie do wykrycia bledow
// mutex przy nickname

const int max_tries = 7;
const int port = 8080;
const int buffer_size = 1024;
std::condition_variable cv;
std::mutex mtx;
std::unique_lock<std::mutex> lck(mtx);

std::vector<std::string> readWordsFromFile(std::string filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cout << "Error: words file not found" << std::endl;
        exit(1);
    }

    std::vector<std::string> words;
    std::string line;
    while (getline(file, line))
    {
        words.push_back(line);
    }
    file.close();

    return words;
}

std::string chooseWord(std::vector<std::string> words)
{
    srand(time(NULL));
    return words[rand() % words.size()];
}

std::string hideWord(std::string word)
{
    std::string hiddenWord(word.length(), '_');
    return hiddenWord;
}

std::vector<std::string> Players;
std::vector<std::string> Winners;
std::mutex nicknameMutex;
std::mutex loggingMutex;

void handleClient(int client_socket, std::string word, std::vector<int> &sockets, int &starter)
{
    // int starter;
    int starter2;
    char buffer[buffer_size] = {0};
    std::string decider;

    int read_value = recv(client_socket, buffer, buffer_size, 0);
    std::string nickname(buffer, read_value);
    std::string conn_accepted = "Connection accepted!\n\n";
    std::cout << "Connection accepted from: " << nickname << std::endl;
    std::cout << Players.size() << std::endl;
    send(client_socket, conn_accepted.c_str(), conn_accepted.length(), 0);

    

    while (true)
    {
        nicknameMutex.lock();
        bool nickname_found = find(Players.begin(), Players.end(), nickname) != Players.end();
        if (nickname_found)
        {
            nicknameMutex.unlock();
            std::cout << "Provided nickname: " << nickname << " has already been taken" << std::endl;
            std::string nickname_taken = "Nickname already taken. Enter a different nickname: ";
            send(client_socket, nickname_taken.c_str(), nickname_taken.length(), 0);
            read_value = recv(client_socket, buffer, buffer_size, 0);
            nickname = std::string(buffer, read_value);
        }
        else
        {
            Players.push_back(nickname);
            nicknameMutex.unlock();
            break;
        }
    }

    int numTries = 0;
    std::string guessedLetters;

    std::string hiddenWord = hideWord(word);

    if (Players.size() > 0)
    {
        loggingMutex.lock();
        int x = sockets[0];
        loggingMutex.unlock();

        if (client_socket == x)
        {
            std::string trigger = "Do you want to start the game?[Y/n]";
            send(client_socket, trigger.c_str(), trigger.length(), 0);
            std::cout << "Asking the player to start the game" << std::endl;
            read_value = recv(client_socket, buffer, buffer_size, 0);
            std::string decider(buffer, read_value);
            std::cout << decider << std::endl;
            if (decider == "Y")
            {
                std::unique_lock<std::mutex> l(loggingMutex);
                starter = 1;
                cv.notify_all();
            }
        }

        else
        {
            std::string others = "Waiting for the first player to start the game...";
            send(client_socket, others.c_str(), others.length(), 0);
            
            std::unique_lock<std::mutex> l(loggingMutex);
            cv.wait(l, [&]{return starter;});
            
        }
    }

    // if (Players.size() > 0)
    // {
    //     std::string lobby = "Lobby:\n";
    //     for (int i = 0; i < Players.size(); i++)
    //     {
    //         lobby = lobby + Players[i] + "\n";
    //     }
    //     send(client_socket, lobby.c_str(), lobby.length(), 0);

    while (numTries < max_tries && hiddenWord != word && starter == 1)
    {
        std::string Game_started = "\nGame Started\n";
        send(client_socket, Game_started.c_str(), Game_started.length(), 0);
        // Current status
        std::string message = "Current word: " + hiddenWord + "\nGuessed letters: " + guessedLetters;
        send(client_socket, message.c_str(), message.length(), 0);

        // Receive a letter from the player
        read_value = recv(client_socket, buffer, buffer_size, 0);
        char letter = buffer[0];

        // Check if the letter has already been guessed
        if (guessedLetters.find(letter) != std::string::npos)
        {
            std::string mess = "You already guessed that letter.\n";
            send(client_socket, mess.c_str(), mess.length(), 0);
        }
        else
        {
            // Add the letter to guessed
            guessedLetters += letter;

            // Check if the letter is in the word
            if (word.find(letter) != std::string::npos)
            {
                for (int i = 0; i < word.length(); i++)
                {
                    if (word[i] == letter)
                    {
                        hiddenWord[i] = letter;
                        if (hiddenWord == word)
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                // Increment tries
                numTries++;
            }
        }
    }
    // Sending the final sttatust of the game to the player
    if (numTries == max_tries)
    {
        std::string lost = "You lost. The word was: " + word + ".";
        send(client_socket, lost.c_str(), lost.length(), 0);
    }
    else
    {
        std::string win = "You won! The word was: " + word;
        send(client_socket, win.c_str(), win.length(), 0);
    }
    Players.erase(find(Players.begin(), Players.end(), nickname));
    close(client_socket);
    // }
}

int main(int argc, char **argv)
{
    std::string ip = "0.0.0.0";
    int port = 8080;

    int server_fd, new_socket, read_value;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    for (int i = 1; i < argc - 1; i++)
    {
        std::string arg(argv[i]);
        if (arg == "-ip")
        {
            i++;
            ip = argv[i];
        }

        if (arg == "-port")
        {
            i++;
            port = std::stoi(argv[i]);
        }
    }

    std::cout << "ip: " << ip << " PORT: " << port << std::endl;

    std::vector<std::string> words = readWordsFromFile("words.txt");
    std::cout << "File with " << words.size() << " clues has been loaded." << std::endl;

    std::string word = chooseWord(words);
    std::cout << "Chosen word: " << word << std::endl;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to the address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::vector<int> sockets;
    int starter = 0;

    while (true)
    {
        std::cout << "Waiting for connection...\n";

        // Accept new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        loggingMutex.lock();
        sockets.push_back(new_socket);

        loggingMutex.unlock();
        // create new thread to handle the client
        std::thread t(handleClient, new_socket, word, std::ref(sockets), std::ref(starter));
        t.detach();
        // while (cv.wait_for(lck, std::chrono::seconds(5)) == std::cv_status::no_timeout)
        // {
        //     std::cout << "Waiting for other players" << std::endl;
        //     starter = 1;
        // }
    }

    return 0;
}