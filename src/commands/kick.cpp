#include "../../include/Server.hpp"
#include <string>

/* https://modern.ircdocs.horse/#kick-message
 * Parameters: <channel> <user> *( "," <user> ) [<comment>] */
void Server::_kick(PollfdIterator it, const std::vector<std::string>& args)
{
 
    Client& client = m_clients.find(it->fd)->second;
	if (args.size() < 2)
		return client.send_461("KICK");

    const std::string& channel_name = args[1];
    bool is_operator = false;	

	ChannelIterator target_channel = m_channels.find(channel_name);
	if (target_channel == m_channels.end())
		return client.send_403(args[1]);
	Channel& channel = target_channel->second;

    std::map<int, Client>::iterator client_it = m_clients.find(it->fd);
    if (_check_presence(target_channel, client_it, is_operator) != 0)
    {
        client.send_442(target_channel->second);
        return;
    }
    if (!is_operator)
    {
        client.send_482(target_channel->second);
        return;
    } 

    std::vector<std::string>  targets;
    _parse_comma_args(args[2], targets);
    
    std::string message = "You have been kicked";
    if (args.size() > 3)
        message = args[3];

    for (std::vector<std::string>::iterator it = targets.begin(); it != targets.end(); it++)
    {
        ClientIterator target_it = _find_client_by_nickname(*it); 
        if (target_it == m_clients.end())
        {
            client.send_msg("Fuck you!");
            continue;
        }
        Client& target = target_it->second;
        if (!channel.is_subscribed(target.fd))
        {
            client.send_442(channel);
            continue;
        }
        channel.send_msg(client.nickname + " KICK " + channel.name + " " + target.nickname);
        channel.remove_client(target);
    }
}
