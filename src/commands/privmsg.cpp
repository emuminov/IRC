#include "../../include/Server.hpp"
#include <iostream>
#include <string>

/* https://modern.ircdocs.horse/#privmsg-message
 * Parameters: <target>{,<target>} <text to be sent> */
void Server::_privmsg(PollfdIterator it, const std::vector<std::string>& args)
{
	if (_check_privmsg_args(it, args) != 0)
		return ;
	std::vector<std::string> targets;
	std::vector<std::string> arg_copy = args;

	_parse_privmsg_args(arg_copy, targets);

	std::map<int, Client>::iterator client = m_clients.find(it->fd);

	std::vector<std::string>::iterator t_it = targets.begin();
	t_it--;
	while (++t_it != targets.end())
	{
		std::map<std::string, Channel>::iterator target_channel = m_channels.find(*t_it);
		if (target_channel != m_channels.end())
		{
			std::vector<int>::iterator c_it = target_channel->second.subscribed_users_fd.begin();
			while (c_it != target_channel->second.subscribed_users_fd.end())
			{
				std::string	msg = ":" + client->second.nickname + " PRIVMSG " + target_channel->first + " :" + args[2] + "\n";
				send(*c_it, msg.c_str(), msg.size(), MSG_CONFIRM);
				c_it++;
			}
			continue;
		}
		ClientIterator target_user = _find_client_by_nickname(*t_it);
		if (target_user != m_clients.end())
		{
			std::string	msg = ":" + client->second.nickname + " PRIVMSG " + target_user->second.nickname + " :" + args[2] + "\n";
			send(target_user->second.fd, msg.c_str(), msg.size(), MSG_CONFIRM);
			continue;
		}
		std::string	err_msg = "User/Channel : " + *t_it + " not found\n";
		_send_to_client(it->fd, "401", err_msg);
	}
}

int Server::_check_privmsg_args(PollfdIterator it, const std::vector<std::string>& args)
{
	if (args.size() == 1)
	{
		Server::_send_to_client(it->fd, "411", "No recipient :\nPRIVMSG <target>{,<target>} : <message>");
		return (1);
	}
	if (args.size() == 2)
	{
		Server::_send_to_client(it->fd, "412", "No message to send :\nPRIVMSG <target>{,<target>} : <message>");
		return (1);
	}
	if (args.size() > 3)
	{
		Server::_send_to_client(it->fd, "407", "Too many parameters :\nPRIVMSG <target>{,<target>} : <message>");
		return (1);
	}
	return (0);
}

void Server::_parse_privmsg_args(std::vector<std::string>& args, std::vector<std::string> & targets)
{
	size_t	pos = 0;
	while (args[1][pos])
	{
		pos = args[1].find(',');
		if (pos == std::string::npos)
		{
			targets.push_back(args[1]);
			return ;
		}
		targets.push_back(args[1].substr(0, pos - 1));
		args[1].erase(0, pos);
	}
}