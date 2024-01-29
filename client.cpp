#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const int port = 8080;

int main(int argc, char **argv)
{
    int sock = 0, read_value;
    struct sockaddr_in server_addr;
    const int buffer_size = 1024;
    char buffer[buffer_size] = {0};
    std::string decider;

    std::string ip = "127.0.0.1";
    int port = 8080;

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
    std::cout << argc << std::endl;

    std::cout <<"ip: " << ip << " PORT: " << port << std::endl;
    // Get nickname
    std::string nickname;
    std::cout << "Welcome to the Hangman game!\nEnter your nickname: ";
    std::cin >> nickname;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cout << "failed to create socket.\n";
        return -1;
    }

    memset(&server_addr, '0', sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert ipv4 and ipv6 addresses from text to binary
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        std::cout << "invalid address.\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cout << "connection failed.\n";
        return -1;
    }

    // send nickanme to the server
    send(sock, nickname.c_str(), nickname.length(), 0);

    // start the game
    while (true)
    {
        // reeive current state of the game form server
        char buffer[buffer_size] = {0};
        read_value = recv(sock, buffer, buffer_size, 0);
        if (read_value == 0)
        {
            std::cout << "Disconnected.\n";
            break;
        }
        std::cout << buffer << std::endl;
        if (strstr(buffer, "You lost") || strstr(buffer, "You won"))
        {
            break;
        };
        if (strstr(buffer, "Nickname already taken."))
        {
            std::cin >> nickname;
            send(sock, nickname.c_str(), nickname.length(), 0);
            continue;
        };
        if (strstr(buffer, "Do you want to start the game"))
        {
            //std::cout << "Do you want to start the game? [Y/n]" <<std::endl;
            std::cin >> decider;
            send(sock, decider.c_str(), decider.length(), 0);
        };

        // get user input
        if (strstr(buffer, "Current word"))
        {
            std::string guess;
            std::cout << "Enter a guess: ";
            std::cin >> guess;
            // send the guess to server
            send(sock, guess.c_str(), guess.length(), 0);
        }
    }
    close(sock);

    return 0;
}