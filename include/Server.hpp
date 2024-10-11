#ifndef SERVER_HPP
#define SERVER_HPP
#include "Channel.hpp"
#include "Client.hpp"
#include <map>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <vector>
#define MAX_CONNECTIONS 16
#define USERLEN 18
#define MESSAGE_SIZE 512
#define POLL_TIMEOUT 10000
class Server
{
  public:
    Server(const char* password, const char* port);
    // Server(const Server&);
    // Server& operator=(const Server&);
    ~Server();

    void start();

  private:
    static bool s_should_run;
    static void _handle_signal(int signum);

    typedef std::vector<struct pollfd>::iterator PollfdIterator;
	typedef std::map<std::string, Channel>::iterator ChannelIterator;
    typedef std::map<int, Client>::iterator ClientIterator;
    void _init_listening_socket();
    void _handle_client_message(PollfdIterator it);
    void _handle_client_connection();
    void _remove_client(PollfdIterator* it);
    void _add_client(int fd);
    void _send_to_client(int fd, std::string error_code, std::string msg);
    ClientIterator _find_client_by_nickname(const std::string& nickname);

    /* All functions for USER cmd */
    void _user(PollfdIterator it, const std::vector<std::string>& args);
    int _check_user_args(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for PASS cmd */
    void _pass(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for NICK cmd */
    void _nick(PollfdIterator it, const std::vector<std::string>& args);
    int _check_nick_args(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for JOIN cmd */
    void _join(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for PRIVMSG cmd */
    void _privmsg(PollfdIterator it, const std::vector<std::string>& args);
    int _check_privmsg_args(PollfdIterator it, const std::vector<std::string>& args);
    void _parse_privmsg_args(std::vector<std::string>& args, std::vector<std::string>& targets);

    /* All functions for LIST cmd */
    void _list(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for NAME cmd */
    void _names(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for NOTICE cmd */
    void _notice(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for QUIT cmd */
    void _quit(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for PING cmd */
    void _ping(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for PONG cmd */
    void _pong(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for INVITE cmd */
    void _invite(PollfdIterator it, const std::vector<std::string>& args);

    /* All functions for TOPIC cmd */
    void _topic(PollfdIterator it, const std::vector<std::string>& args);

    const std::string m_password;
    const std::string m_port;
    std::vector<struct pollfd> m_pfds;
    std::map<int, Client> m_clients;
    std::map<std::string, Channel> m_channels;
    std::map<std::string, void (Server::*)(PollfdIterator, const std::vector<std::string>&)>
        m_commands;
    int m_sockfd;
};
#endif
