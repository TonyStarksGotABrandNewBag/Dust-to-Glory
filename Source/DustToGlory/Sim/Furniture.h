// Homes, furniture stores, delivery scheduling and furniture placement.
#pragma once

#include "Sim/Economy.h"
#include "Sim/SimCommon.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	struct FurnitureItem
	{
		std::string Id;
		std::string Name;
		std::string Store;     // which furniture store sells it
		std::string Category;  // "Living Room", "Bedroom", ...
		Money Price = 0;
		double WidthM = 1.0;
		double DepthM = 1.0;
		double HeightM = 1.0;
		std::uint32_t ColorRgb = 0x8B6F4E;
	};

	struct Property
	{
		std::string Id;
		std::string Name;
		std::string Kind;      // Apartment, House, Mansion
		Money Price = 0;
		int Bedrooms = 1;
		double InteriorWidthM = 10.0;
		double InteriorDepthM = 8.0;
	};

	enum class OrderStatus : int
	{
		Processing,
		OutForDelivery,
		Delivered,
		Cancelled
	};

	const char* OrderStatusName(OrderStatus Status);

	enum class DeliverySpeed : int
	{
		Standard,
		Express
	};

	struct FurnitureOrder
	{
		int Id = 0;
		std::string PropertyId;
		std::vector<std::string> Items;
		Money Subtotal = 0;
		Money DeliveryFee = 0;
		DeliverySpeed Speed = DeliverySpeed::Standard;
		std::int64_t PlacedMinute = 0;
		std::int64_t DispatchMinute = 0;
		std::int64_t DeliverMinute = 0;
		OrderStatus Status = OrderStatus::Processing;
	};

	struct PlacedFurniture
	{
		int Uid = 0;
		std::string ItemId;
		std::string PropertyId;
		double X = 0.0;   // metres from the interior's origin corner
		double Y = 0.0;
		double YawDeg = 0.0;
	};

	struct InventoryEntry
	{
		std::string PropertyId;
		std::string ItemId;
	};

	struct DeliveryEvent
	{
		int OrderId = 0;
		OrderStatus NewStatus = OrderStatus::Processing;
		std::string PropertyId;
	};

	/** Delivery fee for an order subtotal. Standard shipping is free over $1,500. */
	Money DeliveryFeeFor(Money Subtotal, DeliverySpeed Speed);
	/** Delivery lands inside business hours (09:00-18:00). */
	std::int64_t ScheduleDelivery(std::int64_t NowMinute, DeliverySpeed Speed);

	class HomeAndFurniture
	{
	public:
		void AddItem(FurnitureItem Item);
		void AddProperty(Property Value);
		const FurnitureItem* FindItem(const std::string& Id) const;
		const Property* FindProperty(const std::string& Id) const;
		const std::vector<FurnitureItem>& Items() const { return Catalog; }
		const std::vector<Property>& Properties() const { return PropertyList; }
		std::vector<std::string> Stores() const;
		std::vector<const FurnitureItem*> ItemsAtStore(const std::string& Store) const;

		bool OwnsProperty(const std::string& Id) const;
		const std::vector<std::string>& OwnedProperties() const { return Owned; }
		bool BuyProperty(const std::string& Id, Wallet& Funds, std::int64_t NowMinute, std::string* OutError, double PriceMultiplier = 1.0);
		/** The first home is free so the player always has somewhere to put furniture. */
		void GrantProperty(const std::string& Id);

		bool PlaceOrder(const std::vector<std::string>& ItemIds, const std::string& PropertyId, DeliverySpeed Speed,
			Wallet& Funds, std::int64_t NowMinute, int& OutOrderId, std::string* OutError);
		/** Advances order statuses. Orders past their delivery time are completed even if no truck arrived. */
		std::vector<DeliveryEvent> Update(std::int64_t NowMinute);
		/** Called by the delivery truck on arrival; moves items into the home's inventory. */
		bool CompleteDelivery(int OrderId);
		const std::vector<FurnitureOrder>& Orders() const { return OrderList; }
		const FurnitureOrder* FindOrder(int OrderId) const;

		std::vector<std::string> Unplaced(const std::string& PropertyId) const;
		const std::vector<PlacedFurniture>& Placed() const { return PlacedList; }
		/** Places one unplaced copy of the item. Fails if none is waiting in that home. */
		bool Place(const std::string& PropertyId, const std::string& ItemId, double X, double Y, double YawDeg, int& OutUid);
		bool Move(int Uid, double X, double Y, double YawDeg);
		/** Returns the item to the home's inventory. */
		bool PickUp(int Uid);

		/** Save/load. */
		struct Snapshot
		{
			std::vector<std::string> Owned;
			std::vector<FurnitureOrder> Orders;
			std::vector<InventoryEntry> Inventory;
			std::vector<PlacedFurniture> Placed;
			int NextOrderId = 1;
			int NextUid = 1;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		std::vector<FurnitureItem> Catalog;
		std::vector<Property> PropertyList;
		std::vector<std::string> Owned;
		std::vector<FurnitureOrder> OrderList;
		std::vector<InventoryEntry> Inventory;
		std::vector<PlacedFurniture> PlacedList;
		int NextOrderId = 1;
		int NextUid = 1;
	};
}
