// Minimal JSON reader/writer for game data files and save games.
// Shared by the engine and the tests so both interpret data files identically.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	class JsonValue
	{
	public:
		enum class Type
		{
			Null,
			Bool,
			Number,
			String,
			Array,
			Object
		};

		JsonValue() = default;

		static JsonValue MakeBool(bool Value);
		static JsonValue MakeNumber(double Value);
		static JsonValue MakeString(std::string Value);
		static JsonValue MakeArray();
		static JsonValue MakeObject();

		Type GetType() const { return Kind; }
		bool IsNull() const { return Kind == Type::Null; }
		bool IsBool() const { return Kind == Type::Bool; }
		bool IsNumber() const { return Kind == Type::Number; }
		bool IsString() const { return Kind == Type::String; }
		bool IsArray() const { return Kind == Type::Array; }
		bool IsObject() const { return Kind == Type::Object; }

		bool AsBool(bool Fallback = false) const { return Kind == Type::Bool ? BoolValue : Fallback; }
		double AsNumber(double Fallback = 0.0) const { return Kind == Type::Number ? NumberValue : Fallback; }
		int AsInt(int Fallback = 0) const;
		std::int64_t AsInt64(std::int64_t Fallback = 0) const;
		/** Returns an empty string when this is not a string. */
		const std::string& AsString() const;

		/** Element count for arrays and objects, zero otherwise. */
		std::size_t Size() const;

		/** Array element, or a shared null value when out of range / not an array. */
		const JsonValue& At(std::size_t Index) const;
		/** Object member, or nullptr when missing / not an object. */
		const JsonValue* Find(const std::string& Key) const;
		/** Object member, or a shared null value when missing. */
		const JsonValue& Get(const std::string& Key) const;
		bool Has(const std::string& Key) const { return Find(Key) != nullptr; }

		std::string GetString(const std::string& Key, const std::string& Fallback = std::string()) const;
		double GetNumber(const std::string& Key, double Fallback = 0.0) const;
		int GetInt(const std::string& Key, int Fallback = 0) const;
		std::int64_t GetInt64(const std::string& Key, std::int64_t Fallback = 0) const;
		bool GetBool(const std::string& Key, bool Fallback = false) const;

		const std::vector<JsonValue>& Items() const { return ArrayValue; }
		const std::vector<std::pair<std::string, JsonValue>>& Members() const { return ObjectValue; }

		/** Sets or replaces an object member (turns a null value into an object). Returns the stored member. */
		JsonValue& Set(const std::string& Key, JsonValue Value);
		/** Appends to an array (turns a null value into an array). Returns the stored element. */
		JsonValue& Push(JsonValue Value);

		std::string Dump(bool bPretty = false) const;

	private:
		void DumpTo(std::string& Out, bool bPretty, int Depth) const;

		Type Kind = Type::Null;
		bool BoolValue = false;
		double NumberValue = 0.0;
		std::string StringValue;
		std::vector<JsonValue> ArrayValue;
		std::vector<std::pair<std::string, JsonValue>> ObjectValue;
	};

	struct JsonParseResult
	{
		bool bOk = false;
		JsonValue Value;
		std::string Error;
		std::size_t Line = 0;
		std::size_t Column = 0;
	};

	/** Parses standard JSON. Also accepts // line comments so data files can be annotated. */
	JsonParseResult ParseJson(const std::string& Text);

	/** Escapes a string as a JSON string literal including quotes. */
	std::string JsonQuote(const std::string& Text);
}
