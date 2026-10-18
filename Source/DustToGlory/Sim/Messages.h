// The player's phone: calls, text threads and email in one inbox.
#pragma once

#include "Sim/SimCommon.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	enum class Channel : int
	{
		Call,
		Text,
		Email,
		Count
	};
	const char* ChannelName(Channel Via);

	/** What a message is about, so the phone can offer the right buttons. */
	enum class MessageTopic : int
	{
		General,
		CarOffer,      // a car owner answering (accept / counter / decline)
		JobOffer,
		Business,      // consulting requests, ownership reports
		Delivery,
		Politics,
		Hacking,
		Ministry,
		Count
	};

	enum class OfferOutcome : int
	{
		Accepted,
		Countered,
		Declined,
		NotForSale
	};
	const char* OfferOutcomeName(OfferOutcome Outcome);

	struct Message
	{
		int Id = 0;
		Channel Via = Channel::Text;
		MessageTopic Topic = MessageTopic::General;
		std::string FromId;         // NPC mind id or business id ("" for the player)
		std::string FromName;
		std::string FromAddress;    // phone number or email address
		std::string Subject;        // email only
		std::string Text;
		std::string ThreadId;       // groups a conversation, e.g. "car:home-42-0"
		std::string CarId;          // car offers
		OfferOutcome Outcome = OfferOutcome::Declined;
		Money Price = 0;            // accepted price or counter-offer
		std::int64_t Minute = 0;
		bool bFromPlayer = false;
		bool bRead = false;
		bool bAwaitingReply = false; // a deal waiting for the player's yes or no
	};

	class Inbox
	{
	public:
		static constexpr std::size_t MaxMessages = 120;

		/** Adds a message and returns its id. */
		int Post(Message Value);
		Message* Find(int Id);
		const Message* Find(int Id) const;
		const std::vector<Message>& All() const { return List; }
		std::vector<const Message*> Thread(const std::string& ThreadId) const;
		std::vector<const Message*> OfChannel(Channel Via) const;
		int Unread() const;
		void MarkRead(int Id);
		/** Latest message from someone else in a thread (what a reply answers). */
		Message* LatestIncoming(const std::string& ThreadId);
		int NextId() const { return NextIdValue; }
		void Restore(std::vector<Message> InList, int InNextId)
		{
			List = std::move(InList);
			NextIdValue = InNextId;
		}

	private:
		std::vector<Message> List;
		int NextIdValue = 1;
	};
}
