#include "Sim/Furniture.h"

namespace dtg::sim
{
	const char* OrderStatusName(OrderStatus Status)
	{
		switch (Status)
		{
		case OrderStatus::Processing: return "Processing";
		case OrderStatus::OutForDelivery: return "Out for delivery";
		case OrderStatus::Delivered: return "Delivered";
		case OrderStatus::Cancelled: return "Cancelled";
		}
		return "Processing";
	}

	Money DeliveryFeeFor(Money Subtotal, DeliverySpeed Speed)
	{
		if (Speed == DeliverySpeed::Express)
		{
			return Dollars(199) + Subtotal / 20;
		}
		return Subtotal >= Dollars(1500) ? 0 : Dollars(79);
	}

	std::int64_t ScheduleDelivery(std::int64_t NowMinute, DeliverySpeed Speed)
	{
		constexpr std::int64_t Day = 24 * 60;
		constexpr std::int64_t Open = 9 * 60;
		constexpr std::int64_t Close = 18 * 60;
		std::int64_t Target = NowMinute + (Speed == DeliverySpeed::Express ? 3 * 60 : Day);
		const std::int64_t TimeOfDay = Target % Day;
		if (TimeOfDay < Open)
		{
			Target += Open - TimeOfDay;
		}
		else if (TimeOfDay > Close)
		{
			Target += Day - TimeOfDay + Open;
		}
		return Target;
	}

	void HomeAndFurniture::AddItem(FurnitureItem Item)
	{
		Catalog.push_back(std::move(Item));
	}

	void HomeAndFurniture::AddProperty(Property Value)
	{
		PropertyList.push_back(std::move(Value));
	}

	const FurnitureItem* HomeAndFurniture::FindItem(const std::string& Id) const
	{
		for (const FurnitureItem& Item : Catalog)
		{
			if (Item.Id == Id)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	const Property* HomeAndFurniture::FindProperty(const std::string& Id) const
	{
		for (const Property& Value : PropertyList)
		{
			if (Value.Id == Id)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	std::vector<std::string> HomeAndFurniture::Stores() const
	{
		std::vector<std::string> Result;
		for (const FurnitureItem& Item : Catalog)
		{
			bool bKnown = false;
			for (const std::string& Store : Result)
			{
				bKnown = bKnown || Store == Item.Store;
			}
			if (!bKnown)
			{
				Result.push_back(Item.Store);
			}
		}
		return Result;
	}

	std::vector<const FurnitureItem*> HomeAndFurniture::ItemsAtStore(const std::string& Store) const
	{
		std::vector<const FurnitureItem*> Result;
		for (const FurnitureItem& Item : Catalog)
		{
			if (Item.Store == Store)
			{
				Result.push_back(&Item);
			}
		}
		return Result;
	}

	bool HomeAndFurniture::OwnsProperty(const std::string& Id) const
	{
		for (const std::string& Value : Owned)
		{
			if (Value == Id)
			{
				return true;
			}
		}
		return false;
	}

	void HomeAndFurniture::GrantProperty(const std::string& Id)
	{
		if (FindProperty(Id) && !OwnsProperty(Id))
		{
			Owned.push_back(Id);
		}
	}

	bool HomeAndFurniture::BuyProperty(const std::string& Id, Wallet& Funds, std::int64_t NowMinute, std::string* OutError, double PriceMultiplier)
	{
		const Property* Value = FindProperty(Id);
		if (!Value)
		{
			if (OutError) *OutError = "Unknown property.";
			return false;
		}
		if (OwnsProperty(Id))
		{
			if (OutError) *OutError = "You already own this home.";
			return false;
		}
		const Money Price = static_cast<Money>(std::llround(static_cast<double>(Value->Price) * std::max(0.1, PriceMultiplier)));
		if (!Funds.TrySpend(Price, MoneyCategory::Property, "Bought " + Value->Name, NowMinute))
		{
			if (OutError) *OutError = "You need " + FormatMoney(Price) + ".";
			return false;
		}
		Owned.push_back(Id);
		return true;
	}

	bool HomeAndFurniture::PlaceOrder(const std::vector<std::string>& ItemIds, const std::string& PropertyId, DeliverySpeed Speed,
		Wallet& Funds, std::int64_t NowMinute, int& OutOrderId, std::string* OutError)
	{
		if (ItemIds.empty())
		{
			if (OutError) *OutError = "Your cart is empty.";
			return false;
		}
		if (!OwnsProperty(PropertyId))
		{
			if (OutError) *OutError = "Furniture can only be delivered to a home you own.";
			return false;
		}
		Money Subtotal = 0;
		for (const std::string& ItemId : ItemIds)
		{
			const FurnitureItem* Item = FindItem(ItemId);
			if (!Item)
			{
				if (OutError) *OutError = "Unknown item: " + ItemId;
				return false;
			}
			Subtotal += Item->Price;
		}
		const Money Fee = DeliveryFeeFor(Subtotal, Speed);
		if (!Funds.TrySpend(Subtotal + Fee, MoneyCategory::Furniture, "Furniture order", NowMinute))
		{
			if (OutError) *OutError = "You need " + FormatMoney(Subtotal + Fee) + ".";
			return false;
		}
		FurnitureOrder Order;
		Order.Id = NextOrderId++;
		Order.PropertyId = PropertyId;
		Order.Items = ItemIds;
		Order.Subtotal = Subtotal;
		Order.DeliveryFee = Fee;
		Order.Speed = Speed;
		Order.PlacedMinute = NowMinute;
		Order.DeliverMinute = ScheduleDelivery(NowMinute, Speed);
		Order.DispatchMinute = std::max(NowMinute, Order.DeliverMinute - 60);
		Order.Status = OrderStatus::Processing;
		OrderList.push_back(Order);
		OutOrderId = Order.Id;
		return true;
	}

	std::vector<DeliveryEvent> HomeAndFurniture::Update(std::int64_t NowMinute)
	{
		std::vector<DeliveryEvent> Events;
		for (FurnitureOrder& Order : OrderList)
		{
			if (Order.Status == OrderStatus::Processing && NowMinute >= Order.DispatchMinute)
			{
				Order.Status = OrderStatus::OutForDelivery;
				Events.push_back(DeliveryEvent{Order.Id, Order.Status, Order.PropertyId});
			}
			// Grace period: if the truck never made it (player far away), the crew still drops it off.
			if (Order.Status == OrderStatus::OutForDelivery && NowMinute >= Order.DeliverMinute + 30)
			{
				const int Id = Order.Id;
				const std::string Home = Order.PropertyId;
				if (CompleteDelivery(Id))
				{
					Events.push_back(DeliveryEvent{Id, OrderStatus::Delivered, Home});
				}
			}
		}
		return Events;
	}

	bool HomeAndFurniture::CompleteDelivery(int OrderId)
	{
		for (FurnitureOrder& Order : OrderList)
		{
			if (Order.Id == OrderId && Order.Status != OrderStatus::Delivered && Order.Status != OrderStatus::Cancelled)
			{
				Order.Status = OrderStatus::Delivered;
				for (const std::string& ItemId : Order.Items)
				{
					Inventory.push_back(InventoryEntry{Order.PropertyId, ItemId});
				}
				return true;
			}
		}
		return false;
	}

	const FurnitureOrder* HomeAndFurniture::FindOrder(int OrderId) const
	{
		for (const FurnitureOrder& Order : OrderList)
		{
			if (Order.Id == OrderId)
			{
				return &Order;
			}
		}
		return nullptr;
	}

	std::vector<std::string> HomeAndFurniture::Unplaced(const std::string& PropertyId) const
	{
		std::vector<std::string> Result;
		for (const InventoryEntry& Entry : Inventory)
		{
			if (Entry.PropertyId == PropertyId)
			{
				Result.push_back(Entry.ItemId);
			}
		}
		return Result;
	}

	bool HomeAndFurniture::Place(const std::string& PropertyId, const std::string& ItemId, double X, double Y, double YawDeg, int& OutUid)
	{
		for (auto It = Inventory.begin(); It != Inventory.end(); ++It)
		{
			if (It->PropertyId == PropertyId && It->ItemId == ItemId)
			{
				Inventory.erase(It);
				PlacedFurniture Piece;
				Piece.Uid = NextUid++;
				Piece.ItemId = ItemId;
				Piece.PropertyId = PropertyId;
				Piece.X = X;
				Piece.Y = Y;
				Piece.YawDeg = YawDeg;
				PlacedList.push_back(Piece);
				OutUid = Piece.Uid;
				return true;
			}
		}
		return false;
	}

	bool HomeAndFurniture::Move(int Uid, double X, double Y, double YawDeg)
	{
		for (PlacedFurniture& Piece : PlacedList)
		{
			if (Piece.Uid == Uid)
			{
				Piece.X = X;
				Piece.Y = Y;
				Piece.YawDeg = YawDeg;
				return true;
			}
		}
		return false;
	}

	bool HomeAndFurniture::PickUp(int Uid)
	{
		for (auto It = PlacedList.begin(); It != PlacedList.end(); ++It)
		{
			if (It->Uid == Uid)
			{
				Inventory.push_back(InventoryEntry{It->PropertyId, It->ItemId});
				PlacedList.erase(It);
				return true;
			}
		}
		return false;
	}

	HomeAndFurniture::Snapshot HomeAndFurniture::Save() const
	{
		Snapshot Data;
		Data.Owned = Owned;
		Data.Orders = OrderList;
		Data.Inventory = Inventory;
		Data.Placed = PlacedList;
		Data.NextOrderId = NextOrderId;
		Data.NextUid = NextUid;
		return Data;
	}

	void HomeAndFurniture::Load(const Snapshot& Data)
	{
		Owned = Data.Owned;
		OrderList = Data.Orders;
		Inventory = Data.Inventory;
		PlacedList = Data.Placed;
		NextOrderId = std::max(1, Data.NextOrderId);
		NextUid = std::max(1, Data.NextUid);
	}
}
