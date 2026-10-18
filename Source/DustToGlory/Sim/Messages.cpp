#include "Sim/Messages.h"

namespace dtg::sim
{
	const char* ChannelName(Channel Via)
	{
		switch (Via)
		{
		case Channel::Call: return "Call";
		case Channel::Text: return "Text";
		case Channel::Email: return "Email";
		case Channel::Count: break;
		}
		return "Message";
	}

	const char* OfferOutcomeName(OfferOutcome Outcome)
	{
		switch (Outcome)
		{
		case OfferOutcome::Accepted: return "Accepted";
		case OfferOutcome::Countered: return "Countered";
		case OfferOutcome::Declined: return "Declined";
		case OfferOutcome::NotForSale: return "Not for sale";
		}
		return "Declined";
	}

	int Inbox::Post(Message Value)
	{
		Value.Id = NextIdValue++;
		if (Value.bFromPlayer)
		{
			Value.bRead = true;
		}
		List.push_back(std::move(Value));
		if (List.size() > MaxMessages)
		{
			// Drop the oldest message that isn't waiting on the player.
			for (auto It = List.begin(); It != List.end(); ++It)
			{
				if (!It->bAwaitingReply)
				{
					List.erase(It);
					break;
				}
			}
		}
		return List.back().Id;
	}

	Message* Inbox::Find(int Id)
	{
		for (Message& Value : List)
		{
			if (Value.Id == Id)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	const Message* Inbox::Find(int Id) const
	{
		for (const Message& Value : List)
		{
			if (Value.Id == Id)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	std::vector<const Message*> Inbox::Thread(const std::string& ThreadId) const
	{
		std::vector<const Message*> Result;
		for (const Message& Value : List)
		{
			if (Value.ThreadId == ThreadId)
			{
				Result.push_back(&Value);
			}
		}
		return Result;
	}

	std::vector<const Message*> Inbox::OfChannel(Channel Via) const
	{
		std::vector<const Message*> Result;
		for (const Message& Value : List)
		{
			if (Value.Via == Via)
			{
				Result.push_back(&Value);
			}
		}
		return Result;
	}

	int Inbox::Unread() const
	{
		int Count = 0;
		for (const Message& Value : List)
		{
			Count += Value.bRead ? 0 : 1;
		}
		return Count;
	}

	void Inbox::MarkRead(int Id)
	{
		if (Message* Value = Find(Id))
		{
			Value->bRead = true;
		}
	}

	Message* Inbox::LatestIncoming(const std::string& ThreadId)
	{
		for (auto It = List.rbegin(); It != List.rend(); ++It)
		{
			if (It->ThreadId == ThreadId && !It->bFromPlayer)
			{
				return &*It;
			}
		}
		return nullptr;
	}
}
