#include "../include/Server.hpp"
#include "../include/client_msg_parse.hpp"
#include <csignal>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

bool Server::s_should_run = true;

Server::Server(const char* password, const char* port)
    : m_password(password), m_port(port), m_pfds(), m_sockfd(-1)
{
    _init_listening_socket();

    m_pfds.reserve(MAX_CONNECTIONS);
    struct pollfd serv_pfd;
    serv_pfd.fd = m_sockfd;
    serv_pfd.events = POLLIN;
    serv_pfd.revents = 0;
    m_pfds.push_back(serv_pfd);

    /* Commands are case insensitive, hence they are lower case. */
    m_commands.insert(make_pair("join", &Server::_join));
    m_commands.insert(make_pair("names", &Server::_names));
    m_commands.insert(make_pair("nick", &Server::_nick));
    m_commands.insert(make_pair("notice", &Server::_notice));
    m_commands.insert(make_pair("pass", &Server::_pass));
    m_commands.insert(make_pair("privmsg", &Server::_privmsg));
    m_commands.insert(make_pair("user", &Server::_user));
    m_commands.insert(make_pair("invite", &Server::_invite));
    m_commands.insert(make_pair("topic", &Server::_topic));
    m_commands.insert(make_pair("mode", &Server::_mode));
    m_commands.insert(make_pair("kick", &Server::_kick));

    signal(SIGINT, Server::_handle_signal);
}

Server::~Server()
{
    close(m_sockfd);
    for (size_t i = 1; i < m_pfds.size(); i++)
        close(m_pfds[i].fd);
}

void Server::start()
{
    while (Server::s_should_run)
    {
        int poll_res = poll(m_pfds.data(), m_pfds.size(), POLL_TIMEOUT);
        if (poll_res == -1 && s_should_run)
            throw std::runtime_error("Critical error: poll failed");

        for (PollfdIterator it = m_pfds.begin(); it != m_pfds.end(); it++)
        {
			if (!s_should_run && it.base() == NULL)
				break;
            if ((it->revents & POLLIN) && it->fd == m_sockfd)
                _handle_client_connection();
            else if ((it->revents & POLLIN) && it->fd != m_sockfd)
                _handle_client_message(&it);
        }
    }
}

void Server::_handle_client_connection()
{
    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    int new_fd = accept(m_sockfd, (struct sockaddr*)&client_addr, &addrlen);

    if (new_fd == -1 && s_should_run)
    {
        std::cerr << "Error: new connection failed\n";
        return;
    }

    _add_client(new_fd);
    std::cout << "Connection accepted: " << new_fd << "\n";
}

void Server::_handle_client_message(PollfdIterator* it)
{
    Client& client = m_clients[(*it)->fd];
    char buf[MESSAGE_SIZE + 1];
    int bytes_received = recv((*it)->fd, buf, MESSAGE_SIZE, 0);
    if (bytes_received < 0 && s_should_run)
    {
        std::cerr << "Error: recv failed\n";
        return;
    }
    // the client is disconnected
    else if (bytes_received == 0)
    {
        std::cout << "Connection closed: " << (*it)->fd << "\n";
        _remove_client(it, client);
        return;
    }
    buf[bytes_received] = '\0';
    client.buf += buf;
    if (client.buf.length() > MESSAGE_SIZE)
    {
		client.send_417();
        return;
    }

    size_t end_of_msg = client.buf.find("\r\n");
    std::string raw_message;
    while (end_of_msg != std::string::npos)
    {
        raw_message = client.buf.substr(0, end_of_msg);
		std::cout << raw_message << "\n";
        client.buf.erase(0, end_of_msg + 2);

        std::vector<std::string> args = parse_client_msg(raw_message);
        const std::string& command = args[0];
		if (command == "quit")
		{
			return _quit(it, args);
		}
		else if (!client.entered_password && command != "pass")
		{
			client.send_464();
		}
		else if (client.entered_password &&
				(!client.is_registered && !(command == "nick" || command == "user")))
		{
			client.send_451();
		}
		else if (m_commands.find(command) != m_commands.end())
		{
			(this->*m_commands[command])(it, args);
		}
		else
        {
			client.send_421(command);
        }
        end_of_msg = client.buf.find("\r\n");
    }
    return;
}

void Server::_init_listening_socket()
{
    struct addrinfo* ai;
    struct addrinfo hints = {};

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int gai_status = getaddrinfo(NULL, m_port.c_str(), &hints, &ai);
    if (gai_status != 0)
        throw std::runtime_error("Critical error: getaddrinfo failed");
    struct addrinfo* curr;
    for (curr = ai; curr != NULL; curr = curr->ai_next)
    {
        m_sockfd = socket(curr->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (m_sockfd < 0)
            continue;

        int yes = 1;
        if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
        {
            close(m_sockfd);
            continue;
        }

        if (bind(m_sockfd, curr->ai_addr, curr->ai_addrlen) != 0)
        {
            close(m_sockfd);
            continue;
        }

        break;
    }
    freeaddrinfo(ai);
    if (curr == NULL && s_should_run)
        throw std::runtime_error("Critical error: socket binding failed");

    if (listen(m_sockfd, MAX_CONNECTIONS) != 0 && s_should_run)
    {
        close(m_sockfd);
        throw std::runtime_error("Critical error: listen failed");
    }
}

void Server::_remove_client(PollfdIterator* it, Client& client)
{
	for (size_t i = 0; i < client.channels.size(); i++)
	{
		Channel& target_channel = m_channels[client.channels[i]];
		target_channel.remove_client(client);
	}
	close((*it)->fd);
    m_clients.erase((*it)->fd);
    *it = m_pfds.erase(*it);
	std::advance(*it, -1);
}

void Server::_add_client(int fd)
{
    struct pollfd new_pollfd;
    new_pollfd.fd = fd;
    new_pollfd.events = POLLIN;
    new_pollfd.revents = 0;
    Client new_client(fd);
    m_clients.insert(std::make_pair(fd, new_client));
    m_pfds.push_back(new_pollfd);
}

void Server::_send_to_client(int fd, std::string error_code, std::string msg)
{
    std::string total = ":42Chan ";
    std::map<int, Client>::iterator client = m_clients.find(fd);

    total += error_code + " " + client->second.nickname + " :" + msg + "\n";
    send(fd, total.c_str(), total.size(), MSG_CONFIRM);
}

void Server::_send_to_fd(int fd, const std::string& msg)
{
	std::string total = msg + "\r\n";
    send(fd, total.c_str(), total.size(), MSG_CONFIRM);
}

void Server::_send_to_channel_subscribers(const Channel& channel, const std::string& msg)
{
	std::string total = msg + "\r\n";
	for (std::vector<int>::const_iterator it = channel.subscribed_users_fd.begin();
		it != channel.subscribed_users_fd.end(); it++)
	{
		send(*it, total.c_str(), total.size(), MSG_CONFIRM);
	}
}

Server::ClientIterator Server::_find_client_by_nickname(const std::string& nickname)
{
    ClientIterator it;
    for (it = m_clients.begin(); it != m_clients.end(); it++)
    {
        if (it->second.nickname == nickname)
            return it;
    }
    return it;
}

void Server::_remove_client_from_channel(Channel& channel, Client& client)
{
	channel.remove_client(client);
	client.quit_channel(channel.name);
	if (channel.subscribed_users_fd.size() == 0)
	{
		m_channels.erase(channel.name);
	}
}

void Server::_remove_client_from_all_channels(Client& client)
{
	for (size_t i = 0; i < client.channels.size(); i++)
	{
		_remove_client_from_channel(m_channels[client.channels[i]], client);
	}
}

void Server::_handle_signal(int signum)
{
    if (signum == SIGINT)
        Server::s_should_run = false;
}
