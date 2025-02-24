#include "../../include/Server.hpp"
#include "../../include/utils.hpp"

/* https://modern.ircdocs.horse/#topic-message
 * Parameters: <channel> [<topic>] */
void Server::_topic(PollfdIterator* it, const std::vector<std::string>& args)
{
    Client& client = m_clients.find((*it)->fd)->second;
    if (args.size() < 2)
        return client.send_461("TOPIC");

    const std::string& channel_name = args[1];
    ChannelIterator target_channel_it = _find_channel(channel_name);
    if (target_channel_it == m_channels.end())
        return client.send_403(channel_name);

    Channel& channel = target_channel_it->second;
    if (args.size() == 2)
    {
        if (channel.topic == "")
            return client.send_331(channel);
        client.send_332(channel);
        client.send_333(channel);
        return;
    }

    if (!channel.is_subscribed(client.fd))
        return client.send_442(channel);

    if (channel.is_const_topic_mode && !channel.is_operator(client.fd))
        return client.send_482(channel);

    channel.topic = args[2];
    channel.topic_timestampt = long_to_str(time(NULL));
    channel.topic_starter_nickname = client.nickname;
    channel.send_msg(":" + client.nickname + " TOPIC " + channel.name + " :" + channel.topic);
}
